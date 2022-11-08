// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/background_readback.h"

#include "base/numerics/safe_conversions.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame_pool.h"
#include "media/base/video_util.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_init.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace {
bool CanUseRgbReadback(media::VideoFrame& frame) {
  return media::IsRGB(frame.format()) && (frame.NumTextures() == 1);
}

gpu::raster::RasterInterface* GetSharedGpuRasterInterface() {
  auto wrapper = blink::SharedGpuContext::ContextProviderWrapper();
  if (wrapper && wrapper->ContextProvider()) {
    auto* raster_provider = wrapper->ContextProvider()->RasterContextProvider();
    if (raster_provider)
      return raster_provider->RasterInterface();
  }
  return nullptr;
}
}  // namespace

namespace blink {

// This is a part of BackgroundReadback that lives and dies on the worker's
// thread and does all the actual work of creating GPU context and calling
// sync readback functions.
class SyncReadbackThread
    : public WTF::ThreadSafeRefCounted<SyncReadbackThread> {
 public:
  SyncReadbackThread();
  scoped_refptr<media::VideoFrame> Readback(
      scoped_refptr<media::VideoFrame> frame);

 private:
  bool LazyInitialize();
  media::VideoFramePool result_frame_pool_;
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider_;
  THREAD_CHECKER(thread_checker_);
};

BackgroundReadback::BackgroundReadback(base::PassKey<BackgroundReadback> key,
                                       ExecutionContext& context)
    : Supplement<ExecutionContext>(context),
      sync_readback_impl_(base::MakeRefCounted<SyncReadbackThread>()),
      worker_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::WithBaseSyncPrimitives()},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)) {}

BackgroundReadback::~BackgroundReadback() {
  worker_task_runner_->ReleaseSoon(FROM_HERE, std::move(sync_readback_impl_));
}

const char BackgroundReadback::kSupplementName[] = "BackgroundReadback";
// static
BackgroundReadback* BackgroundReadback::From(ExecutionContext& context) {
  BackgroundReadback* supplement =
      Supplement<ExecutionContext>::From<BackgroundReadback>(context);
  if (!supplement) {
    supplement = MakeGarbageCollected<BackgroundReadback>(
        base::PassKey<BackgroundReadback>(), context);
    Supplement<ExecutionContext>::ProvideTo(context, supplement);
  }
  return supplement;
}

void BackgroundReadback::ReadbackTextureBackedFrameToMemory(
    scoped_refptr<media::VideoFrame> txt_frame,
    ReadbackDoneCallback result_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(txt_frame);

  if (CanUseRgbReadback(*txt_frame)) {
    ReadbackRGBTextureBackedFrameToMemory(std::move(txt_frame),
                                          std::move(result_cb));
    return;
  }
  ReadbackOnThread(std::move(txt_frame), std::move(result_cb));
}

void BackgroundReadback::ReadbackOnThread(
    scoped_refptr<media::VideoFrame> txt_frame,
    ReadbackDoneCallback result_cb) {
  worker_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      ConvertToBaseOnceCallback(
          CrossThreadBindOnce(&SyncReadbackThread::Readback,
                              sync_readback_impl_, std::move(txt_frame))),
      std::move(result_cb));
}

void BackgroundReadback::ReadbackRGBTextureBackedFrameToMemory(
    scoped_refptr<media::VideoFrame> txt_frame,
    ReadbackDoneCallback result_cb) {
  DCHECK(CanUseRgbReadback(*txt_frame));

  SkImageInfo info = SkImageInfo::MakeN32(txt_frame->coded_size().width(),
                                          txt_frame->coded_size().height(),
                                          kUnpremul_SkAlphaType);
  const auto format = media::VideoPixelFormatFromSkColorType(
      info.colorType(), media::IsOpaque(txt_frame->format()));

  auto result = result_frame_pool_.CreateFrame(
      format, txt_frame->coded_size(), txt_frame->visible_rect(),
      txt_frame->natural_size(), txt_frame->timestamp());

  auto* ri = GetSharedGpuRasterInterface();
  if (!ri || !result) {
    media::BindToCurrentLoop(std::move(std::move(result_cb))).Run(nullptr);
    return;
  }

  uint8_t* dst_pixels =
      result->GetWritableVisibleData(media::VideoFrame::kARGBPlane);
  int rgba_stide = result->stride(media::VideoFrame::kARGBPlane);
  DCHECK_GT(rgba_stide, 0);

  auto origin = txt_frame->metadata().texture_origin_is_top_left
                    ? kTopLeft_GrSurfaceOrigin
                    : kBottomLeft_GrSurfaceOrigin;

  gfx::Point src_point;
  gpu::MailboxHolder mailbox_holder = txt_frame->mailbox_holder(0);
  ri->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());
  ri->ReadbackARGBPixelsAsync(
      mailbox_holder.mailbox, mailbox_holder.texture_target, origin, src_point,
      info, base::saturated_cast<GLuint>(rgba_stide), dst_pixels,
      WTF::BindOnce(&BackgroundReadback::OnARGBPixelsReadCompleted,
                    MakeUnwrappingCrossThreadHandle(this), std::move(result_cb),
                    std::move(txt_frame), std::move(result)));
}

void BackgroundReadback::OnARGBPixelsReadCompleted(
    ReadbackDoneCallback result_cb,
    scoped_refptr<media::VideoFrame> txt_frame,
    scoped_refptr<media::VideoFrame> result_frame,
    bool success) {
  if (!success) {
    ReadbackOnThread(std::move(txt_frame), std::move(result_cb));
    return;
  }
  if (auto* ri = GetSharedGpuRasterInterface()) {
    media::WaitAndReplaceSyncTokenClient client(ri);
    txt_frame->UpdateReleaseSyncToken(&client);
  }

  result_frame->set_color_space(txt_frame->ColorSpace());
  result_frame->metadata().MergeMetadataFrom(txt_frame->metadata());
  result_frame->metadata().ClearTextureFrameMedatada();
  std::move(result_cb).Run(success ? std::move(result_frame) : nullptr);
}

SyncReadbackThread::SyncReadbackThread() {
  DETACH_FROM_THREAD(thread_checker_);
}

bool SyncReadbackThread::LazyInitialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (context_provider_)
    return true;
  Platform::ContextAttributes attributes;
  attributes.enable_raster_interface = true;
  attributes.support_grcontext = true;
  attributes.prefer_low_power_gpu = true;

  Platform::GraphicsInfo info;
  context_provider_ = CreateOffscreenGraphicsContext3DProvider(
      attributes, &info, KURL("chrome://BackgroundReadback"));

  if (!context_provider_) {
    DLOG(ERROR) << "Can't create context provider.";
    return false;
  }

  if (!context_provider_->BindToCurrentSequence()) {
    DLOG(ERROR) << "Can't bind context provider.";
    context_provider_ = nullptr;
    return false;
  }
  return true;
}

scoped_refptr<media::VideoFrame> SyncReadbackThread::Readback(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!LazyInitialize())
    return nullptr;

  auto* ri = context_provider_->RasterInterface();
  auto* gr_context = context_provider_->GetGrContext();
  return media::ReadbackTextureBackedFrameToMemorySync(*frame, ri, gr_context,
                                                       &result_frame_pool_);
}

}  // namespace blink
