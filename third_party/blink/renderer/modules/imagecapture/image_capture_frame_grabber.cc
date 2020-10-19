// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/imagecapture/image_capture_frame_grabber.h"

#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "skia/ext/legacy_display_globals.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace WTF {
// Template specialization of [1], needed to be able to pass callbacks
// that have ScopedWebCallbacks paramaters across threads.
//
// [1] third_party/blink/renderer/platform/wtf/cross_thread_copier.h.
template <typename T>
struct CrossThreadCopier<blink::ScopedWebCallbacks<T>>
    : public CrossThreadCopierPassThrough<blink::ScopedWebCallbacks<T>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = blink::ScopedWebCallbacks<T>;
  static blink::ScopedWebCallbacks<T> Copy(
      blink::ScopedWebCallbacks<T> pointer) {
    return pointer;
  }
};

}  // namespace WTF

namespace blink {

namespace {

void OnError(std::unique_ptr<ImageCaptureGrabFrameCallbacks> callbacks) {
  callbacks->OnError();
}

}  // anonymous namespace

// Ref-counted class to receive a single VideoFrame on IO thread, convert it and
// send it to |main_task_runner_|, where this class is created and destroyed.
class ImageCaptureFrameGrabber::SingleShotFrameHandler
    : public WTF::ThreadSafeRefCounted<SingleShotFrameHandler> {
 public:
  SingleShotFrameHandler() : first_frame_received_(false) {}

  // Receives a |frame| and converts its pixels into a SkImage via an internal
  // PaintSurface and SkPixmap. Alpha channel, if any, is copied.
  using SkImageDeliverCB = WTF::CrossThreadFunction<void(sk_sp<SkImage>)>;
  void OnVideoFrameOnIOThread(
      SkImageDeliverCB callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<media::VideoFrame> frame,
      base::TimeTicks current_time);

 private:
  friend class WTF::ThreadSafeRefCounted<SingleShotFrameHandler>;

  // Flag to indicate that the first frames has been processed, and subsequent
  // ones can be safely discarded.
  bool first_frame_received_;

  DISALLOW_COPY_AND_ASSIGN(SingleShotFrameHandler);
};

void ImageCaptureFrameGrabber::SingleShotFrameHandler::OnVideoFrameOnIOThread(
    SkImageDeliverCB callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks /* current_time */) {
  DCHECK(frame->format() == media::PIXEL_FORMAT_I420 ||
         frame->format() == media::PIXEL_FORMAT_I420A ||
         frame->format() == media::PIXEL_FORMAT_NV12);

  if (first_frame_received_)
    return;
  first_frame_received_ = true;

  const SkAlphaType alpha = media::IsOpaque(frame->format())
                                ? kOpaque_SkAlphaType
                                : kPremul_SkAlphaType;
  const SkImageInfo info = SkImageInfo::MakeN32(
      frame->visible_rect().width(), frame->visible_rect().height(), alpha);

  SkSurfaceProps props = skia::LegacyDisplayGlobals::GetSkSurfaceProps();
  sk_sp<SkSurface> surface = SkSurface::MakeRaster(info, &props);
  DCHECK(surface);

  auto wrapper_callback =
      media::BindToLoop(std::move(task_runner),
                        ConvertToBaseRepeatingCallback(std::move(callback)));

  SkPixmap pixmap;
  if (!skia::GetWritablePixels(surface->getCanvas(), &pixmap)) {
    DLOG(ERROR) << "Error trying to map SkSurface's pixels";
    std::move(wrapper_callback).Run(sk_sp<SkImage>());
    return;
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

  if (frame->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    auto* gmb = frame->GetGpuMemoryBuffer();
    if (!gmb->Map()) {
      DLOG(ERROR) << "Error mapping GpuMemoryBuffer video frame";
      std::move(wrapper_callback).Run(sk_sp<SkImage>());
      return;
    }

    // NV12 is the only supported pixel format at the moment.
    DCHECK_EQ(frame->format(), media::PIXEL_FORMAT_NV12);
    int y_stride = gmb->stride(0);
    int uv_stride = gmb->stride(1);
    const uint8_t* y_plane =
        (static_cast<uint8_t*>(gmb->memory(0)) + frame->visible_rect().x() +
         (frame->visible_rect().y() * y_stride));
    // UV plane of NV12 has 2-byte pixel width, with half chroma subsampling
    // both horizontally and vertically.
    const uint8_t* uv_plane = (static_cast<uint8_t*>(gmb->memory(1)) +
                               ((frame->visible_rect().x() * 2) / 2) +
                               ((frame->visible_rect().y() / 2) * uv_stride));

    switch (destination_pixel_format) {
      case libyuv::FOURCC_ABGR:
        libyuv::NV12ToABGR(y_plane, y_stride, uv_plane, uv_stride,
                           destination_plane, destination_stride,
                           destination_width, destination_height);
        break;
      case libyuv::FOURCC_ARGB:
        libyuv::NV12ToARGB(y_plane, y_stride, uv_plane, uv_stride,
                           destination_plane, destination_stride,
                           destination_width, destination_height);
        break;
      default:
        NOTREACHED();
    }
    gmb->Unmap();
  } else {
    DCHECK(frame->format() == media::PIXEL_FORMAT_I420 ||
           frame->format() == media::PIXEL_FORMAT_I420A);
    libyuv::ConvertFromI420(frame->visible_data(media::VideoFrame::kYPlane),
                            frame->stride(media::VideoFrame::kYPlane),
                            frame->visible_data(media::VideoFrame::kUPlane),
                            frame->stride(media::VideoFrame::kUPlane),
                            frame->visible_data(media::VideoFrame::kVPlane),
                            frame->stride(media::VideoFrame::kVPlane),
                            destination_plane, destination_stride,
                            destination_width, destination_height,
                            destination_pixel_format);

    if (frame->format() == media::PIXEL_FORMAT_I420A) {
      DCHECK(!info.isOpaque());
      // This function copies any plane into the alpha channel of an ARGB image.
      libyuv::ARGBCopyYToAlpha(frame->visible_data(media::VideoFrame::kAPlane),
                               frame->stride(media::VideoFrame::kAPlane),
                               destination_plane, destination_stride,
                               destination_width, destination_height);
    }
  }

  std::move(wrapper_callback).Run(surface->makeImageSnapshot());
}

ImageCaptureFrameGrabber::ImageCaptureFrameGrabber()
    : frame_grab_in_progress_(false) {}

ImageCaptureFrameGrabber::~ImageCaptureFrameGrabber() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ImageCaptureFrameGrabber::GrabFrame(
    MediaStreamComponent* component,
    std::unique_ptr<ImageCaptureGrabFrameCallbacks> callbacks,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!!callbacks);

  DCHECK(component && component->GetPlatformTrack());
  DCHECK_EQ(MediaStreamSource::kTypeVideo, component->Source()->GetType());

  if (frame_grab_in_progress_) {
    // Reject grabFrame()s too close back to back.
    callbacks->OnError();
    return;
  }

  auto scoped_callbacks =
      MakeScopedWebCallbacks(std::move(callbacks), WTF::Bind(&OnError));

  // A SingleShotFrameHandler is bound and given to the Track to guarantee that
  // only one VideoFrame is converted and delivered to OnSkImage(), otherwise
  // SKImages might be sent to resolved |callbacks| while DisconnectFromTrack()
  // is being processed, which might be further held up if UI is busy, see
  // https://crbug.com/623042.
  frame_grab_in_progress_ = true;
  MediaStreamVideoSink::ConnectToTrack(
      WebMediaStreamTrack(component),
      ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
          &SingleShotFrameHandler::OnVideoFrameOnIOThread,
          base::MakeRefCounted<SingleShotFrameHandler>(),
          WTF::Passed(CrossThreadBindRepeating(
              &ImageCaptureFrameGrabber::OnSkImage, weak_factory_.GetWeakPtr(),
              WTF::Passed(std::move(scoped_callbacks)))),
          WTF::Passed(std::move(task_runner)))),
      false);
}

void ImageCaptureFrameGrabber::OnSkImage(
    ScopedWebCallbacks<ImageCaptureGrabFrameCallbacks> callbacks,
    sk_sp<SkImage> image) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  MediaStreamVideoSink::DisconnectFromTrack();
  frame_grab_in_progress_ = false;
  if (image)
    callbacks.PassCallbacks()->OnSuccess(image);
  else
    callbacks.PassCallbacks()->OnError();
}

}  // namespace blink
