// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/imagecapture/image_capture_frame_grabber.h"

#include "base/compiler_specific.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "cc/paint/skia_paint_canvas.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "skia/ext/legacy_display_globals.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

// Helper that ensures `resolver` is always rejected on `task_runner` if it is
// not consumed.
class ImageCaptureFrameGrabber::ScopedPromiseResolver {
 public:
  ScopedPromiseResolver(ScriptPromiseResolver<ImageBitmap>* resolver,
                        scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : resolver_(MakeUnwrappingCrossThreadHandle(resolver)),
        task_runner_(std::move(task_runner)) {}

  ~ScopedPromiseResolver() {
    if (task_runner_) {
      using PromiseType = ScriptPromiseResolver<ImageBitmap>;
      PostCrossThreadTask(
          *task_runner_, FROM_HERE,
          CrossThreadBindOnce(
              static_cast<void (PromiseType::*)()>(&PromiseType::Reject),
              std::move(resolver_)));
    }
  }

  ScopedPromiseResolver(ScopedPromiseResolver&& other) = default;
  ScopedPromiseResolver(const ScopedPromiseResolver& other) = delete;

  // `resolver_` has a deleted move assignment operator.
  ScopedPromiseResolver& operator=(ScopedPromiseResolver&& other) = delete;
  ScopedPromiseResolver& operator=(const ScopedPromiseResolver& other) = delete;

  UnwrappingCrossThreadHandle<ScriptPromiseResolver<ImageBitmap>>
  TakeResolver() && {
    task_runner_ = nullptr;
    return std::move(resolver_);
  }

 private:
  UnwrappingCrossThreadHandle<ScriptPromiseResolver<ImageBitmap>> resolver_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

// Helper class that receives a single VideoFrame on the IO thread.
class ImageCaptureFrameGrabber::SingleShotFrameHandler {
 public:
  explicit SingleShotFrameHandler(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::WeakPtr<ImageCaptureFrameGrabber> frame_grabber,
      ScopedPromiseResolver resolver)
      : task_runner_(std::move(task_runner)),
        frame_grabber_(std::move(frame_grabber)),
        resolver_(std::move(resolver)) {}

  SingleShotFrameHandler(const SingleShotFrameHandler&) = delete;
  SingleShotFrameHandler& operator=(const SingleShotFrameHandler&) = delete;

  ~SingleShotFrameHandler();

  // Receives a |frame| and converts its pixels into a SkImage via an internal
  // PaintSurface and SkPixmap. Alpha channel, if any, is copied.
  void OnVideoFrameOnIOThread(scoped_refptr<media::VideoFrame> frame,
                              base::TimeTicks current_time);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<ImageCaptureFrameGrabber> frame_grabber_;
  ScopedPromiseResolver resolver_;
};

ImageCaptureFrameGrabber::SingleShotFrameHandler::~SingleShotFrameHandler() {
  // ~ScopedPromiseResolver will reject if still needed.
}

void ImageCaptureFrameGrabber::SingleShotFrameHandler::OnVideoFrameOnIOThread(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks /*current_time*/) {
  if (!task_runner_) {
    return;
  }

  PostCrossThreadTask(
      *std::exchange(task_runner_, nullptr), FROM_HERE,
      CrossThreadBindOnce(&ImageCaptureFrameGrabber::OnVideoFrame,
                          std::move(frame_grabber_),
                          base::RetainedRef(std::move(frame)),
                          std::move(resolver_).TakeResolver()));
}

// TODO(dcheng): Move this to the unnamed namespace. Left here for now to
// simplify reviewing.
static sk_sp<SkImage> ConvertFrame(media::VideoFrame* frame) {
  media::VideoRotation rotation = media::VIDEO_ROTATION_0;
  if (frame->metadata().transformation) {
    rotation = frame->metadata().transformation->rotation;
  }

  const gfx::Size& original_size = frame->visible_rect().size();
  gfx::Size display_size = original_size;
  if (rotation == media::VIDEO_ROTATION_90 ||
      rotation == media::VIDEO_ROTATION_270) {
    display_size.SetSize(display_size.height(), display_size.width());
  }
  const SkAlphaType alpha = media::IsOpaque(frame->format())
                                ? kOpaque_SkAlphaType
                                : kPremul_SkAlphaType;
  const SkImageInfo info =
      SkImageInfo::MakeN32(display_size.width(), display_size.height(), alpha);

  SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info, &props);
  DCHECK(surface);

  // If a frame is GPU backed, we need to use PaintCanvasVideoRenderer to read
  // it back from the GPU.
  const bool is_readable = frame->format() == media::PIXEL_FORMAT_I420 ||
                           frame->format() == media::PIXEL_FORMAT_I420A ||
                           (frame->format() == media::PIXEL_FORMAT_NV12 &&
                            frame->HasMappableGpuBuffer());
  if (!is_readable) {
    cc::SkiaPaintCanvas canvas(surface->getCanvas());
    cc::PaintFlags paint_flags;
    DrawVideoFrameIntoCanvas(std::move(frame), &canvas, paint_flags,
                             /*ignore_video_transformation=*/false);
    return surface->makeImageSnapshot();
  }

  SkPixmap pixmap;
  if (!skia::GetWritablePixels(surface->getCanvas(), &pixmap)) {
    DLOG(ERROR) << "Error trying to map SkSurface's pixels";
    return nullptr;
  }

#if SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
  const uint32_t destination_pixel_format = libyuv::FOURCC_ABGR;
#else
  const uint32_t destination_pixel_format = libyuv::FOURCC_ARGB;
#endif
  uint8_t* destination_plane = static_cast<uint8_t*>(pixmap.writable_addr());
  int destination_stride = pixmap.width() * 4;
  int destination_width = pixmap.width();
  int destination_height = pixmap.height();

  // The frame rotating code path based on libyuv will convert any format to
  // I420, rotate under I420 and transform I420 to destination format.
  bool need_rotate = rotation != media::VIDEO_ROTATION_0;
  scoped_refptr<media::VideoFrame> i420_frame;

  if (frame->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    DCHECK_EQ(frame->format(), media::PIXEL_FORMAT_NV12);
    auto scoped_mapping = frame->MapGMBOrSharedImage();
    if (!scoped_mapping) {
      DLOG(ERROR) << "Failed to get the mapped memory.";
      return nullptr;
    }

    // NV12 is the only supported pixel format at the moment.
    DCHECK_EQ(frame->format(), media::PIXEL_FORMAT_NV12);
    size_t y_stride = scoped_mapping->Stride(media::VideoFrame::Plane::kY);
    size_t uv_stride = scoped_mapping->Stride(media::VideoFrame::Plane::kUV);
    auto y_plane = scoped_mapping->GetMemoryAsSpan(media::VideoFrame::Plane::kY)
                       .subspan(frame->visible_rect().x() +
                                (frame->visible_rect().y() * y_stride));
    // UV plane of NV12 has 2-byte pixel width, with half chroma subsampling
    // both horizontally and vertically.
    auto uv_plane =
        scoped_mapping->GetMemoryAsSpan(media::VideoFrame::Plane::kUV)
            .subspan(((frame->visible_rect().x() * 2) / 2) +
                     ((frame->visible_rect().y() / 2) * uv_stride));

    if (need_rotate) {
      // Transform to I420 first to be later on rotated.
      i420_frame = media::VideoFrame::CreateFrame(
          media::PIXEL_FORMAT_I420, original_size, gfx::Rect(original_size),
          original_size, base::TimeDelta());

      libyuv::NV12ToI420(
          y_plane.data(), y_stride, uv_plane.data(), uv_stride,
          i420_frame->GetWritableVisibleData(media::VideoFrame::Plane::kY),
          i420_frame->stride(media::VideoFrame::Plane::kY),
          i420_frame->GetWritableVisibleData(media::VideoFrame::Plane::kU),
          i420_frame->stride(media::VideoFrame::Plane::kU),
          i420_frame->GetWritableVisibleData(media::VideoFrame::Plane::kV),
          i420_frame->stride(media::VideoFrame::Plane::kV),
          original_size.width(), original_size.height());
    } else {
      switch (destination_pixel_format) {
        case libyuv::FOURCC_ABGR:
          libyuv::NV12ToABGR(y_plane.data(), y_stride, uv_plane.data(),
                             uv_stride, destination_plane, destination_stride,
                             destination_width, destination_height);
          break;
        case libyuv::FOURCC_ARGB:
          libyuv::NV12ToARGB(y_plane.data(), y_stride, uv_plane.data(),
                             uv_stride, destination_plane, destination_stride,
                             destination_width, destination_height);
          break;
        default:
          NOTREACHED();
      }
    }
  } else {
    DCHECK(frame->format() == media::PIXEL_FORMAT_I420 ||
           frame->format() == media::PIXEL_FORMAT_I420A);
    i420_frame = std::move(frame);
  }

  if (i420_frame) {
    if (need_rotate) {
      scoped_refptr<media::VideoFrame> rotated_frame =
          media::VideoFrame::CreateFrame(media::PIXEL_FORMAT_I420, display_size,
                                         gfx::Rect(display_size), display_size,
                                         base::TimeDelta());

      libyuv::RotationMode libyuv_rotate = [rotation]() {
        switch (rotation) {
          case media::VIDEO_ROTATION_0:
            return libyuv::kRotate0;
          case media::VIDEO_ROTATION_90:
            return libyuv::kRotate90;
          case media::VIDEO_ROTATION_180:
            return libyuv::kRotate180;
          case media::VIDEO_ROTATION_270:
            return libyuv::kRotate270;
        }
      }();

      libyuv::I420Rotate(
          i420_frame->visible_data(media::VideoFrame::Plane::kY),
          i420_frame->stride(media::VideoFrame::Plane::kY),
          i420_frame->visible_data(media::VideoFrame::Plane::kU),
          i420_frame->stride(media::VideoFrame::Plane::kU),
          i420_frame->visible_data(media::VideoFrame::Plane::kV),
          i420_frame->stride(media::VideoFrame::Plane::kV),
          rotated_frame->GetWritableVisibleData(media::VideoFrame::Plane::kY),
          rotated_frame->stride(media::VideoFrame::Plane::kY),
          rotated_frame->GetWritableVisibleData(media::VideoFrame::Plane::kU),
          rotated_frame->stride(media::VideoFrame::Plane::kU),
          rotated_frame->GetWritableVisibleData(media::VideoFrame::Plane::kV),
          rotated_frame->stride(media::VideoFrame::Plane::kV),
          original_size.width(), original_size.height(), libyuv_rotate);
      i420_frame = std::move(rotated_frame);
    }

    libyuv::ConvertFromI420(
        i420_frame->visible_data(media::VideoFrame::Plane::kY),
        i420_frame->stride(media::VideoFrame::Plane::kY),
        i420_frame->visible_data(media::VideoFrame::Plane::kU),
        i420_frame->stride(media::VideoFrame::Plane::kU),
        i420_frame->visible_data(media::VideoFrame::Plane::kV),
        i420_frame->stride(media::VideoFrame::Plane::kV), destination_plane,
        destination_stride, destination_width, destination_height,
        destination_pixel_format);

    if (i420_frame->format() == media::PIXEL_FORMAT_I420A) {
      DCHECK(!info.isOpaque());
      // This function copies any plane into the alpha channel of an ARGB image.
      libyuv::ARGBCopyYToAlpha(
          i420_frame->visible_data(media::VideoFrame::Plane::kA),
          i420_frame->stride(media::VideoFrame::Plane::kA), destination_plane,
          destination_stride, destination_width, destination_height);
    }
  }

  return surface->makeImageSnapshot();
}

ImageCaptureFrameGrabber::~ImageCaptureFrameGrabber() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ImageCaptureFrameGrabber::GrabFrame(
    MediaStreamComponent* component,
    ScriptPromiseResolver<ImageBitmap>* resolver,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::TimeDelta timeout) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(resolver);

  DCHECK(component && component->GetPlatformTrack());
  DCHECK_EQ(MediaStreamSource::kTypeVideo, component->GetSourceType());

  if (frame_grab_in_progress_) {
    // Reject grabFrame()s too close back to back.
    resolver->Reject();
    return;
  }

  ScopedPromiseResolver scoped_resolver(resolver, task_runner);
  // A SingleShotFrameHandler is bound and given to the Track to guarantee that
  // only one VideoFrame is converted and delivered to OnSkImage(), otherwise
  // SKImages might be sent to resolved |callbacks| while DisconnectFromTrack()
  // is being processed, which might be further held up if UI is busy, see
  // https://crbug.com/623042.
  frame_grab_in_progress_ = true;

  // Fail the grabFrame request if no frame is received for some time to prevent
  // the promise from hanging indefinitely if no frame is ever produced.
  timeout_task_handle_ = PostDelayedCancellableTask(
      *task_runner, FROM_HERE,
      blink::BindOnce(&ImageCaptureFrameGrabber::OnTimeout,
                      weak_factory_.GetWeakPtr()),
      timeout);

  MediaStreamVideoSink::ConnectToTrack(
      WebMediaStreamTrack(component),
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &SingleShotFrameHandler::OnVideoFrameOnIOThread,
          std::make_unique<SingleShotFrameHandler>(
              std::move(task_runner), weak_factory_.GetWeakPtr(),
              std::move(scoped_resolver)))),
      MediaStreamVideoSink::IsSecure::kNo,
      MediaStreamVideoSink::UsesAlpha::kDefault);
}

void ImageCaptureFrameGrabber::OnVideoFrame(
    media::VideoFrame* frame,
    ScriptPromiseResolver<ImageBitmap>* resolver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  sk_sp<SkImage> image = ConvertFrame(frame);

  timeout_task_handle_.Cancel();
  MediaStreamVideoSink::DisconnectFromTrack();
  frame_grab_in_progress_ = false;

  if (image) {
    resolver->Resolve(MakeGarbageCollected<ImageBitmap>(
        UnacceleratedStaticBitmapImage::Create(std::move(image))));
  } else {
    resolver->Reject();
  }
}

void ImageCaptureFrameGrabber::OnTimeout() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (frame_grab_in_progress_) {
    MediaStreamVideoSink::DisconnectFromTrack();
    frame_grab_in_progress_ = false;
  }
}

}  // namespace blink
