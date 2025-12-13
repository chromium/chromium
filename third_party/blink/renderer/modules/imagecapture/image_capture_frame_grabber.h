// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_IMAGE_CAPTURE_FRAME_GRABBER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_IMAGE_CAPTURE_FRAME_GRABBER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "third_party/blink/public/platform/web_callbacks.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/graphics/canvas_snapshot_provider.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkImage;

namespace blink {

class ImageBitmap;
class MediaStreamComponent;

// This class grabs Video Frames from a given Media Stream Video Track, binding
// a method of an ephemeral SingleShotFrameHandler every time grabFrame() is
// called. This method receives an incoming media::VideoFrame on a background
// thread and converts it into the appropriate SkBitmap which is sent back to
// OnSkBitmap(). This class is single threaded throughout.
class ImageCaptureFrameGrabber final : public MediaStreamVideoSink {
 public:
  // Helper class to ensure an ImageBitmap promise is always resolved.
  class ScopedPromiseResolver;

  ImageCaptureFrameGrabber() = default;

  ImageCaptureFrameGrabber(const ImageCaptureFrameGrabber&) = delete;
  ImageCaptureFrameGrabber& operator=(const ImageCaptureFrameGrabber&) = delete;

  ~ImageCaptureFrameGrabber() override;

  void GrabFrame(MediaStreamComponent* component,
                 ScriptPromiseResolver<ImageBitmap>* resolver,
                 scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 base::TimeDelta timeout);

 private:
  // Internal class to receive, convert and forward one frame.
  class SingleShotFrameHandler;

  void OnVideoFrame(media::VideoFrame* frame,
                    ScriptPromiseResolver<ImageBitmap>* resolver);
  void OnTimeout();

  // Flag to indicate that there is a frame grabbing in progress.
  bool frame_grab_in_progress_ = false;
  TaskHandle timeout_task_handle_;

  media::PaintCanvasVideoRenderer video_renderer_;
  std::unique_ptr<CanvasSnapshotProvider> snapshot_provider_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<ImageCaptureFrameGrabber> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_IMAGE_CAPTURE_FRAME_GRABBER_H_
