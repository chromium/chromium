// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/background_readback.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/video_frame_converter.h"
#include "media/base/video_frame_converter_internals.h"
#include "media/base/video_frame_pool.h"
#include "media/base/video_util.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_init.h"
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
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace {
bool PrefersExternalSampler(const media::VideoFrame& frame) {
  return frame.HasSharedImage() &&
         frame.shared_image()->format().PrefersExternalSampler();
}

bool CanUseRgbReadback(const media::VideoFrame& frame) {
  return media::IsRGB(frame.format()) && frame.HasSharedImage();
}

bool CanUseRgbReadback(const media::VideoFrame& frame,
                       const media::VideoFrame& dest_frame) {
  return frame.format() == dest_frame.format() && CanUseRgbReadback(frame);
}

bool CanUseYuvReadback(const media::VideoFrame& frame,
                       const media::VideoFrame& dest_frame) {
  if (!frame.HasSharedImage()) {
    return false;
  }
  if (frame.format() != dest_frame.format() ||
      (dest_frame.format() != media::PIXEL_FORMAT_NV12 &&
       dest_frame.format() != media::PIXEL_FORMAT_I420)) {
    return false;
  }
  // ReadbackYUVPixelsAsync requires 2x2 alignment for origin and size.
  const gfx::Rect& visible_rect = frame.visible_rect();
  if (visible_rect.x() % 2 != 0 || visible_rect.y() % 2 != 0 ||
      visible_rect.width() % 2 != 0 || visible_rect.height() % 2 != 0) {
    return false;
  }
  return true;
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
  if (wrapper) {
    return wrapper->ContextProvider().RasterInterface();
  }
  return nullptr;
}

bool ReadbackTextureBackedFrameSyncImpl(media::VideoFrame& src_frame,
                                        media::VideoFrame& dest_frame,
                                        gpu::raster::RasterInterface* ri,
                                        media::VideoFramePool* pool,
                                        media::VideoFrameConverter& converter) {
  if (!ri) {
    return false;
  }

  if (PrefersExternalSampler(src_frame) ||
      src_frame.format() != dest_frame.format()) {
    auto memory_frame =
        media::ReadbackTextureBackedFrameToMemorySync(src_frame, ri, pool);
    if (!memory_frame) {
      return false;
    }
    if (memory_frame->format() == dest_frame.format()) {
      return blink::BackgroundReadback::CopyMappablePlanes(*memory_frame,
                                                           dest_frame);
    }
    return converter.ConvertAndScale(*memory_frame, dest_frame).is_ok();
  }

  for (size_t i = 0; i < dest_frame.layout().num_planes(); i++) {
    const gfx::Size sample_size =
        media::VideoFrame::SampleSize(dest_frame.format(), i);
    gfx::Rect plane_src_rect =
        blink::PlaneRect(src_frame.visible_rect(), sample_size);
    uint8_t* dest_pixels = dest_frame.GetWritableVisibleData(i);
    if (!media::ReadbackTexturePlaneToMemorySync(src_frame, i, plane_src_rect,
                                                 dest_pixels,
                                                 dest_frame.stride(i), ri)) {
      return false;
    }
  }
  return true;
}

}  // namespace

namespace blink {

// This is a part of BackgroundReadback that lives and dies on the worker's
// thread and does all the actual work of creating GPU context and calling
// sync readback functions.
class SyncReadbackThread : public ThreadSafeRefCounted<SyncReadbackThread> {
 public:
  SyncReadbackThread();
  scoped_refptr<media::VideoFrame> ReadbackToFrame(
      scoped_refptr<media::VideoFrame> frame);

  bool Readback(scoped_refptr<media::VideoFrame> frame,
                scoped_refptr<media::VideoFrame> dest_frame);

 private:
  bool LazyInitialize();
  media::VideoFramePool result_frame_pool_;
  media::VideoFrameConverter converter_;
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider_;
  THREAD_CHECKER(thread_checker_);
};

BackgroundReadback::BackgroundReadback(base::PassKey<BackgroundReadback> key,
                                       ExecutionContext& context)
    : Supplement<ExecutionContext>(context),
      task_runner_(context.GetTaskRunner(TaskType::kInternalMedia)),
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

// static
bool BackgroundReadback::CopyMappablePlanes(const media::VideoFrame& src_frame,
                                            media::VideoFrame& dest_frame) {
  if (!src_frame.HasDirectCpuAccess() || !dest_frame.HasDirectCpuAccess() ||
      src_frame.format() != dest_frame.format() ||
      src_frame.visible_rect().size() != dest_frame.visible_rect().size()) {
    return false;
  }
  media::internals::CopyVisiblePlanes(src_frame, dest_frame);
  return true;
}

bool BackgroundReadback::ReadbackTextureBackedFrameSync(
    media::VideoFrame& txt_frame,
    media::VideoFrame& dest_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* ri = GetSharedGpuRasterInterface();
  return ReadbackTextureBackedFrameSyncImpl(txt_frame, dest_frame, ri,
                                            &result_frame_pool_, converter_);
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

void BackgroundReadback::ReadbackTextureBackedFrame(
    scoped_refptr<media::VideoFrame> txt_frame,
    scoped_refptr<media::VideoFrame> dest_frame,
    ReadbackDoneCallback done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(txt_frame);
  DCHECK(dest_frame);

  if (CanUseRgbReadback(*txt_frame, *dest_frame)) {
    ReadbackRGBTextureBackedFrame(txt_frame, dest_frame, std::move(done_cb));
    return;
  }
  if (CanUseYuvReadback(*txt_frame, *dest_frame)) {
    ReadbackYUVTextureBackedFrame(txt_frame, dest_frame, std::move(done_cb));
    return;
  }
  ReadbackOnThread(std::move(txt_frame), std::move(dest_frame),
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
    scoped_refptr<media::VideoFrame> dest_frame,
    ReadbackDoneCallback done_cb) {
  worker_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          &SyncReadbackThread::Readback, sync_readback_impl_,
          std::move(txt_frame), std::move(dest_frame))),
      std::move(done_cb));
}

void BackgroundReadback::ReadbackRGBTextureBackedFrameToMemory(
    scoped_refptr<media::VideoFrame> txt_frame,
    ReadbackToFrameDoneCallback result_cb) {
  DCHECK(CanUseRgbReadback(*txt_frame));

  SkImageInfo info =
      GetImageInfoForFrame(*txt_frame, txt_frame->visible_rect().size());
  const auto format = media::VideoPixelFormatFromSkColorType(
      info.colorType(), media::IsOpaque(txt_frame->format()));

  auto result = result_frame_pool_.CreateFrame(
      format, txt_frame->coded_size(), txt_frame->visible_rect(),
      txt_frame->natural_size(), txt_frame->timestamp());

  auto* ri = GetSharedGpuRasterInterface();
  if (!ri || !result) {
    base::BindPostTask(task_runner_, std::move(result_cb)).Run(nullptr);
    return;
  }

  TRACE_EVENT_BEGIN("media", "ReadbackRGBTextureBackedFrameToMemory",
                    perfetto::NamedTrack::FromPointer(
                        "blink::BackgroundReadback", txt_frame.get()),
                    "timestamp", txt_frame->timestamp());

  base::span<uint8_t> dst_pixels =
      result->GetWritableVisiblePlaneData(media::VideoFrame::Plane::kARGB);
  int rgba_stride =
      static_cast<int>(result->stride(media::VideoFrame::Plane::kARGB));
  DCHECK_GT(rgba_stride, 0);

  gfx::Point src_point = txt_frame->visible_rect().origin();
  auto shared_image = txt_frame->shared_image();
  auto origin = shared_image->surface_origin();
  std::unique_ptr<gpu::RasterScopedAccess> ri_access =
      shared_image->BeginRasterAccess(ri, txt_frame->acquire_sync_token(),
                                      /*readonly=*/true);

  gfx::Size texture_size = txt_frame->coded_size();
  ri->ReadbackARGBPixelsAsync(
      shared_image->mailbox(), shared_image->GetTextureTarget(), origin,
      texture_size, src_point, info, base::saturated_cast<GLuint>(rgba_stride),
      dst_pixels,
      blink::BindOnce(&BackgroundReadback::OnARGBPixelsFrameReadCompleted,
                      MakeUnwrappingCrossThreadWeakHandle(this),
                      std::move(result_cb), txt_frame, std::move(result)));
  media::WaitAndReplaceSyncTokenClient client(ri, std::move(ri_access));
  txt_frame->UpdateReleaseSyncToken(&client);
}

void BackgroundReadback::OnARGBPixelsFrameReadCompleted(
    ReadbackToFrameDoneCallback result_cb,
    scoped_refptr<media::VideoFrame> txt_frame,
    scoped_refptr<media::VideoFrame> result_frame,
    bool success) {
  TRACE_EVENT_END("media",
                  perfetto::NamedTrack::FromPointer("blink::BackgroundReadback",
                                                    txt_frame.get()),
                  "success", success);
  if (!success) {
    ReadbackOnThread(std::move(txt_frame), std::move(result_cb));
    return;
  }

  auto* ri = GetSharedGpuRasterInterface();

  result_frame->set_color_space(txt_frame->ColorSpace());
  result_frame->set_hdr_metadata(txt_frame->hdr_metadata());
  result_frame->metadata().MergeMetadataFrom(txt_frame->metadata());
  result_frame->metadata().ClearTextureFrameMetadata();
  std::move(result_cb).Run(ri ? std::move(result_frame) : nullptr);
}

void BackgroundReadback::ReadbackRGBTextureBackedFrame(
    scoped_refptr<media::VideoFrame> txt_frame,
    scoped_refptr<media::VideoFrame> dest_frame,
    ReadbackDoneCallback done_cb) {
  if (dest_frame->layout().num_planes() != 1) {
    NOTREACHED()
        << "This method shouldn't be called on anything but RGB frames";
  }

  auto* ri = GetSharedGpuRasterInterface();
  if (!ri) {
    ReadbackOnThread(std::move(txt_frame), std::move(dest_frame),
                     std::move(done_cb));
    return;
  }

  base::span<uint8_t> dst_pixels =
      dest_frame->GetWritableVisiblePlaneData(media::VideoFrame::Plane::kARGB);
  int rgba_stride = base::checked_cast<int>(
      dest_frame->stride(media::VideoFrame::Plane::kARGB));
  const size_t visible_rows =
      base::checked_cast<size_t>(dest_frame->visible_rect().height());
  base::CheckedNumeric<size_t> required_bytes = 0;
  if (visible_rows > 0) {
    required_bytes =
        base::CheckedNumeric<size_t>(rgba_stride) * (visible_rows - 1) +
        dest_frame->GetVisibleRowBytes(media::VideoFrame::Plane::kARGB);
  }
  if (rgba_stride <= 0 || !required_bytes.IsValid() ||
      required_bytes.ValueOrDie() > dst_pixels.size()) {
    DLOG(ERROR) << "Buffer is not sufficiently large for readback";
    base::BindPostTask(task_runner_, std::move(done_cb)).Run(false);
    return;
  }

  TRACE_EVENT_BEGIN("media", "ReadbackRGBTextureBackedFrameToBuffer",
                    perfetto::NamedTrack::FromPointer(
                        "blink::BackgroundReadback", txt_frame.get()),
                    "timestamp", txt_frame->timestamp());

  SkImageInfo info =
      GetImageInfoForFrame(*txt_frame, txt_frame->visible_rect().size());
  gfx::Point src_point = txt_frame->visible_rect().origin();
  auto shared_image = txt_frame->shared_image();
  auto origin = shared_image->surface_origin();
  auto ri_access = shared_image->BeginRasterAccess(
      ri, txt_frame->acquire_sync_token(), /*readonly=*/true);

  auto completion_cb =
      blink::BindOnce(&BackgroundReadback::OnARGBPixelsReadCompleted,
                      MakeUnwrappingCrossThreadWeakHandle(this), txt_frame,
                      dest_frame, std::move(done_cb));

  gfx::Size texture_size = txt_frame->coded_size();
  ri->ReadbackARGBPixelsAsync(
      shared_image->mailbox(), shared_image->GetTextureTarget(), origin,
      texture_size, src_point, info, base::saturated_cast<GLuint>(rgba_stride),
      dst_pixels, std::move(completion_cb));
  media::WaitAndReplaceSyncTokenClient client(ri, std::move(ri_access));
  txt_frame->UpdateReleaseSyncToken(&client);
}

void BackgroundReadback::OnARGBPixelsReadCompleted(
    scoped_refptr<media::VideoFrame> txt_frame,
    scoped_refptr<media::VideoFrame> dest_frame,
    ReadbackDoneCallback done_cb,
    bool success) {
  TRACE_EVENT_END("media",
                  perfetto::NamedTrack::FromPointer("blink::BackgroundReadback",
                                                    txt_frame.get()),
                  "success", success);
  if (!success) {
    ReadbackOnThread(std::move(txt_frame), std::move(dest_frame),
                     std::move(done_cb));
    return;
  }

  std::move(done_cb).Run(true);
}

void BackgroundReadback::ReadbackYUVTextureBackedFrame(
    scoped_refptr<media::VideoFrame> txt_frame,
    scoped_refptr<media::VideoFrame> dest_frame,
    ReadbackDoneCallback done_cb) {
  DCHECK(CanUseYuvReadback(*txt_frame, *dest_frame));

  auto* ri = GetSharedGpuRasterInterface();
  if (!ri) {
    ReadbackOnThread(std::move(txt_frame), std::move(dest_frame),
                     std::move(done_cb));
    return;
  }

  scoped_refptr<media::VideoFrame> triplanar_frame;
  if (dest_frame->format() == media::PIXEL_FORMAT_NV12) {
    triplanar_frame = converter_.WrapBiplanarFrameInTriplanarFrame(*dest_frame);
    if (!triplanar_frame) {
      ReadbackOnThread(std::move(txt_frame), std::move(dest_frame),
                       std::move(done_cb));
      return;
    }
  } else {
    DCHECK_EQ(dest_frame->format(), media::PIXEL_FORMAT_I420);
    triplanar_frame = dest_frame;
  }

  TRACE_EVENT_BEGIN("media", "ReadbackYUVTextureBackedFrameToBuffer",
                    perfetto::NamedTrack::FromPointer(
                        "blink::BackgroundReadback", txt_frame.get()),
                    "timestamp", txt_frame->timestamp());

  auto shared_image = txt_frame->shared_image();
  auto ri_access = shared_image->BeginRasterAccess(
      ri, txt_frame->acquire_sync_token(), /*readonly=*/true);

  auto completion_cb =
      blink::BindOnce(&BackgroundReadback::OnYUVReadCompleted,
                      MakeUnwrappingCrossThreadWeakHandle(this), txt_frame,
                      dest_frame, triplanar_frame, std::move(done_cb));

  ri->ReadbackYUVPixelsAsync(
      shared_image->mailbox(), shared_image->GetTextureTarget(),
      txt_frame->visible_rect(), gfx::Rect(txt_frame->visible_rect().size()),
      shared_image->surface_origin() != kTopLeft_GrSurfaceOrigin,
      base::checked_cast<int>(
          triplanar_frame->stride(media::VideoFrame::Plane::kY)),
      triplanar_frame->GetWritableVisiblePlaneData(
          media::VideoFrame::Plane::kY),
      base::checked_cast<int>(
          triplanar_frame->stride(media::VideoFrame::Plane::kU)),
      triplanar_frame->GetWritableVisiblePlaneData(
          media::VideoFrame::Plane::kU),
      base::checked_cast<int>(
          triplanar_frame->stride(media::VideoFrame::Plane::kV)),
      triplanar_frame->GetWritableVisiblePlaneData(
          media::VideoFrame::Plane::kV),
      base::DoNothing(), std::move(completion_cb));

  media::WaitAndReplaceSyncTokenClient client(ri, std::move(ri_access));
  txt_frame->UpdateReleaseSyncToken(&client);
}

void BackgroundReadback::OnYUVReadCompleted(
    scoped_refptr<media::VideoFrame> txt_frame,
    scoped_refptr<media::VideoFrame> dest_frame,
    scoped_refptr<media::VideoFrame> triplanar_frame,
    ReadbackDoneCallback done_cb,
    bool success) {
  TRACE_EVENT_END("media",
                  perfetto::NamedTrack::FromPointer("blink::BackgroundReadback",
                                                    txt_frame.get()),
                  "success", success);
  if (!success) {
    ReadbackOnThread(std::move(txt_frame), std::move(dest_frame),
                     std::move(done_cb));
    return;
  }

  if (dest_frame->format() == media::PIXEL_FORMAT_NV12) {
    media::internals::MergeUV(*triplanar_frame, *dest_frame);
  }

  std::move(done_cb).Run(true);
}

SyncReadbackThread::SyncReadbackThread() {
  DETACH_FROM_THREAD(thread_checker_);
}

bool SyncReadbackThread::LazyInitialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (context_provider_)
    return true;
  context_provider_ = CreateRasterGraphicsContextProvider(
      KURL("chrome://BackgroundReadback"),
      Platform::RasterContextType::kWebCodecsReadback);

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
  return media::ReadbackTextureBackedFrameToMemorySync(*frame, ri,
                                                       &result_frame_pool_);
}

bool SyncReadbackThread::Readback(scoped_refptr<media::VideoFrame> frame,
                                  scoped_refptr<media::VideoFrame> dest_frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!frame || !dest_frame || !LazyInitialize()) {
    return false;
  }

  TRACE_EVENT1("media", "SyncReadbackThread::Readback", "timestamp",
               frame->timestamp());

  auto* ri = context_provider_->RasterInterface();
  if (!ri) {
    return false;
  }

  return ReadbackTextureBackedFrameSyncImpl(*frame, *dest_frame, ri,
                                            &result_frame_pool_, converter_);
}

}  // namespace blink
