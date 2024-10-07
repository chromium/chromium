// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webcodecs/background_readback.h"

#include "base/feature_list.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/video_frame_pool.h"
#include "media/base/video_util.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_init.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_rect_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_util.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_gfx.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace {
bool CanUseRgbReadback(media::VideoFrame& frame) {
  return media::IsRGB(frame.format()) && frame.HasSharedImage();
}

SkImageInfo GetImageInfoForFrame(const media::VideoFrame& frame,
                                 const gfx::Size& size) {
  SkColorType color_type =
      SkColorTypeForPlane(frame.format(), media::VideoFrame::Plane::kARGB);
  SkAlphaType alpha_type = kUnpremul_SkAlphaType;
  return SkImageInfo::Make(size.width(), size.height(), color_type, alpha_type);
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

namespace WTF {

template <>
struct CrossThreadCopier<blink::VideoFrameLayout>
    : public CrossThreadCopierPassThrough<blink::VideoFrameLayout> {
  STATIC_ONLY(CrossThreadCopier);
};

template <>
struct CrossThreadCopier<base::span<uint8_t>>
    : public CrossThreadCopierPassThrough<base::span<uint8_t>> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

// This is a part of BackgroundReadback that lives and dies on the worker's
// thread and does all the actual work of creating GPU context and calling
// sync readback functions.
class SyncReadbackThread
    : public WTF::ThreadSafeRefCounted<SyncReadbackThread> {
 public:
  SyncReadbackThread();
  scoped_refptr<media::VideoFrame> ReadbackToFrame(
      scoped_refptr<media::VideoFrame> frame);

  bool ReadbackToBuffer(scoped_refptr<media::VideoFrame> frame,
                        const gfx::Rect src_rect,
                        const VideoFrameLayout dest_layout,
                        base::span<uint8_t> dest_buffer);

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

void BackgroundReadback::ReadbackTextureBackedFrameToMemoryFrame(
    scoped_refptr<media::VideoFrame> txt_frame,
    ReadbackToFrameDoneCallback result_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(txt_frame);

  if (CanUseRgbReadback(*txt_frame)) {
    ReadbackRGBTextureBackedFrameToMemory(std::move(txt_frame),
                                          std::move(result_cb));
    return;
  }
  ReadbackOnThread(std::move(txt_frame), std::move(result_cb));
}

void BackgroundReadback::ReadbackTextureBackedFrameToBuffer(
    scoped_refptr<media::VideoFrame> txt_frame,
    const gfx::Rect& src_rect,
    const VideoFrameLayout& dest_layout,
    base::span<uint8_t> dest_buffer,
    ReadbackDoneCallback done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(txt_frame);

  if (CanUseRgbReadback(*txt_frame)) {
    ReadbackRGBTextureBackedFrameToBuffer(txt_frame, src_rect, dest_layout,
                                          dest_buffer, std::move(done_cb));
    return;
  }
  ReadbackOnThread(std::move(txt_frame), src_rect, dest_layout, dest_buffer,
                   std::move(done_cb));
}

void BackgroundReadback::ReadbackOnThread(
    scoped_refptr<media::VideoFrame> txt_frame,
    ReadbackToFrameDoneCallback result_cb) {
  worker_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      ConvertToBaseOnceCallback(
          CrossThreadBindOnce(&SyncReadbackThread::ReadbackToFrame,
                              sync_readback_impl_, std::move(txt_frame))),
      std::move(result_cb));
}

void BackgroundReadback::ReadbackOnThread(
    scoped_refptr<media::VideoFrame> txt_frame,
    const gfx::Rect& src_rect,
    const VideoFrameLayout& dest_layout,
    base::span<uint8_t> dest_buffer,
    ReadbackDoneCallback done_cb) {
  worker_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          &SyncReadbackThread::ReadbackToBuffer, sync_readback_impl_,
          std::move(txt_frame), src_rect, dest_layout, dest_buffer)),
      std::move(done_cb));
}

void BackgroundReadback::ReadbackRGBTextureBackedFrameToMemory(
    scoped_refptr<media::VideoFrame> txt_frame,
    ReadbackToFrameDoneCallback result_cb) {
  DCHECK(CanUseRgbReadback(*txt_frame));

  SkImageInfo info = GetImageInfoForFrame(*txt_frame, txt_frame->coded_size());
  const auto format = media::VideoPixelFormatFromSkColorType(
      info.colorType(), media::IsOpaque(txt_frame->format()));

  auto result = result_frame_pool_.CreateFrame(
      format, txt_frame->coded_size(), txt_frame->visible_rect(),
      txt_frame->natural_size(), txt_frame->timestamp());

  auto* ri = GetSharedGpuRasterInterface();
  if (!ri || !result) {
    base::BindPostTaskToCurrentDefault(std::move(std::move(result_cb)))
        .Run(nullptr);
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "media", "ReadbackRGBTextureBackedFrameToMemory", txt_frame.get(),
      "timestamp", txt_frame->timestamp());

  uint8_t* dst_pixels =
      result->GetWritableVisibleData(media::VideoFrame::Plane::kARGB);
  int rgba_stide = result->stride(media::VideoFrame::Plane::kARGB);
  DCHECK_GT(rgba_stide, 0);

  auto origin = txt_frame->metadata().texture_origin_is_top_left
                    ? kTopLeft_GrSurfaceOrigin
                    : kBottomLeft_GrSurfaceOrigin;

  gfx::Point src_point;
  auto shared_image = txt_frame->shared_image();
  ri->WaitSyncTokenCHROMIUM(txt_frame->acquire_sync_token().GetConstData());

  gfx::Size texture_size = txt_frame->coded_size();
  ri->ReadbackARGBPixelsAsync(
      shared_image->mailbox(), shared_image->GetTextureTarget(), origin,
      texture_size, src_point, info, base::saturated_cast<GLuint>(rgba_stide),
      dst_pixels,
      WTF::BindOnce(&BackgroundReadback::OnARGBPixelsFrameReadCompleted,
                    WrapWeakPersistent(this), std::move(result_cb),
                    std::move(txt_frame), std::move(result)));
}

void BackgroundReadback::OnARGBPixelsFrameReadCompleted(
    ReadbackToFrameDoneCallback result_cb,
    scoped_refptr<media::VideoFrame> txt_frame,
    scoped_refptr<media::VideoFrame> result_frame,
    bool success) {
  TRACE_EVENT_NESTABLE_ASYNC_END1("media",
                                  "ReadbackRGBTextureBackedFrameToMemory",
                                  txt_frame.get(), "success", success);
  if (!success) {
    ReadbackOnThread(std::move(txt_frame), std::move(result_cb));
    return;
  }
  if (auto* ri = GetSharedGpuRasterInterface()) {
    media::WaitAndReplaceSyncTokenClient client(ri);
    txt_frame->UpdateReleaseSyncToken(&client);
  } else {
    success = false;
  }

  result_frame->set_color_space(txt_frame->ColorSpace());
  result_frame->metadata().MergeMetadataFrom(txt_frame->metadata());
  result_frame->metadata().ClearTextureFrameMetadata();
  std::move(result_cb).Run(success ? std::move(result_frame) : nullptr);
}

void BackgroundReadback::ReadbackRGBTextureBackedFrameToBuffer(
    scoped_refptr<media::VideoFrame> txt_frame,
    const gfx::Rect& src_rect,
    const VideoFrameLayout& dest_layout,
    base::span<uint8_t> dest_buffer,
    ReadbackDoneCallback done_cb) {
  if (dest_layout.NumPlanes() != 1) {
    NOTREACHED_IN_MIGRATION()
        << "This method shouldn't be called on anything but RGB frames";
    base::BindPostTaskToCurrentDefault(std::move(std::move(done_cb)))
        .Run(false);
    return;
  }

  auto* ri = GetSharedGpuRasterInterface();
  if (!ri) {
    base::BindPostTaskToCurrentDefault(std::move(std::move(done_cb)))
        .Run(false);
    return;
  }

  uint32_t offset = dest_layout.Offset(0);
  uint32_t stride = dest_layout.Stride(0);

  uint8_t* dst_pixels = dest_buffer.data() + offset;
  size_t max_bytes_written = stride * src_rect.height();
  if (stride <= 0 || max_bytes_written > dest_buffer.size()) {
    DLOG(ERROR) << "Buffer is not sufficiently large for readback";
    base::BindPostTaskToCurrentDefault(std::move(std::move(done_cb)))
        .Run(false);
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "media", "ReadbackRGBTextureBackedFrameToBuffer", txt_frame.get(),
      "timestamp", txt_frame->timestamp());

  SkImageInfo info = GetImageInfoForFrame(*txt_frame, src_rect.size());
  gfx::Point src_point = src_rect.origin();
  auto origin = txt_frame->metadata().texture_origin_is_top_left
                    ? kTopLeft_GrSurfaceOrigin
                    : kBottomLeft_GrSurfaceOrigin;

  auto shared_image = txt_frame->shared_image();
  ri->WaitSyncTokenCHROMIUM(txt_frame->acquire_sync_token().GetConstData());

  gfx::Size texture_size = txt_frame->coded_size();
  ri->ReadbackARGBPixelsAsync(
      shared_image->mailbox(), shared_image->GetTextureTarget(), origin,
      texture_size, src_point, info, base::saturated_cast<GLuint>(stride),
      dst_pixels,
      WTF::BindOnce(&BackgroundReadback::OnARGBPixelsBufferReadCompleted,
                    WrapWeakPersistent(this), std::move(txt_frame), src_rect,
                    dest_layout, dest_buffer, std::move(done_cb)));
}

void BackgroundReadback::OnARGBPixelsBufferReadCompleted(
    scoped_refptr<media::VideoFrame> txt_frame,
    const gfx::Rect& src_rect,
    const VideoFrameLayout& dest_layout,
    base::span<uint8_t> dest_buffer,
    ReadbackDoneCallback done_cb,
    bool success) {
  TRACE_EVENT_NESTABLE_ASYNC_END1("media",
                                  "ReadbackRGBTextureBackedFrameToBuffer",
                                  txt_frame.get(), "success", success);
  if (!success) {
    ReadbackOnThread(std::move(txt_frame), src_rect, dest_layout, dest_buffer,
                     std::move(done_cb));
    return;
  }

  if (auto* ri = GetSharedGpuRasterInterface()) {
    media::WaitAndReplaceSyncTokenClient client(ri);
    txt_frame->UpdateReleaseSyncToken(&client);
  } else {
    success = false;
  }

  std::move(done_cb).Run(success);
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

scoped_refptr<media::VideoFrame> SyncReadbackThread::ReadbackToFrame(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!LazyInitialize())
    return nullptr;

  auto* ri = context_provider_->RasterInterface();
  return media::ReadbackTextureBackedFrameToMemorySync(
      *frame, ri, context_provider_->GetCapabilities(), &result_frame_pool_);
}

bool SyncReadbackThread::ReadbackToBuffer(
    scoped_refptr<media::VideoFrame> frame,
    const gfx::Rect src_rect,
    const VideoFrameLayout dest_layout,
    base::span<uint8_t> dest_buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT1("media", "SyncReadbackThread::ReadbackToBuffer", "timestamp",
               frame->timestamp());

  if (!LazyInitialize() || !frame)
    return false;

  auto* ri = context_provider_->RasterInterface();
  if (!ri)
    return false;

  for (wtf_size_t i = 0; i < dest_layout.NumPlanes(); i++) {
    const gfx::Size sample_size =
        media::VideoFrame::SampleSize(dest_layout.Format(), i);
    gfx::Rect plane_src_rect = PlaneRect(src_rect, sample_size);
    uint8_t* dest_pixels = dest_buffer.data() + dest_layout.Offset(i);
    if (!media::ReadbackTexturePlaneToMemorySync(
            *frame, i, plane_src_rect, dest_pixels, dest_layout.Stride(i), ri,
            context_provider_->GetCapabilities())) {
      // It's possible to fail after copying some but not all planes, leaving
      // the output buffer in a corrupt state D:
      return false;
    }
  }

  return true;
}

}  // namespace blink
