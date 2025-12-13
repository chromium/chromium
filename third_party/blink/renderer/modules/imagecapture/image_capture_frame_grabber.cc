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

  const SkAlphaType alpha_type = media::IsOpaque(frame->format())
                                     ? kOpaque_SkAlphaType
                                     : kPremul_SkAlphaType;
  const gfx::ColorSpace dest_color_space = frame->CompatRGBColorSpace();
  if (!snapshot_provider_ ||
      snapshot_provider_->Size() != frame->natural_size() ||
      snapshot_provider_->GetColorSpace() != dest_color_space ||
      snapshot_provider_->GetAlphaType() != alpha_type) {
    snapshot_provider_ = CreateSnapshotProviderForVideoFrame(
        frame->natural_size(),
        viz::SkColorTypeToSinglePlaneSharedImageFormat(kN32_SkColorType),
        alpha_type, dest_color_space,
        // TODO(crbug.com/468035607): The RasterContextProvider is nullptr since
        // this API has historically provided software backed images, but maybe
        // shouldn't be.
        /*raster_context_provider=*/nullptr);
  }

  scoped_refptr<StaticBitmapImage> image = CreateImageFromVideoFrame(
      frame, snapshot_provider_.get(), &video_renderer_);

  timeout_task_handle_.Cancel();
  MediaStreamVideoSink::DisconnectFromTrack();
  frame_grab_in_progress_ = false;

  if (image) {
    resolver->Resolve(MakeGarbageCollected<ImageBitmap>(std::move(image)));
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
