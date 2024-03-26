// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_IMAGE_CAPTURE_FRAME_GRABBER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_IMAGE_CAPTURE_FRAME_GRABBER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/public/platform/web_callbacks.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkImage;

namespace blink {

class ImageBitmap;
class MediaStreamComponent;

// A ScopedWebCallbacks is a move-only scoper which helps manage the lifetime of
// a blink::WebCallbacks object. This is particularly useful when you're
// simultaneously dealing with the following two conditions:
//
//   1. Your WebCallbacks implementation requires either onSuccess or onError to
//      be called before it's destroyed. This is the case with
//      CallbackPromiseAdapter for example, because its underlying
//      ScriptPromiseResolverBase must be resolved or rejected before
//      destruction.
//
//   2. You are passing ownership of the WebCallbacks to code which may
//      silently drop it. A common way for this to happen is to bind the
//      WebCallbacks as an argument to a base::{Once, Repeating}Callback which
//      gets destroyed before it can run.
//
// While it's possible to individually track the lifetime of pending
// WebCallbacks, this becomes cumbersome when dealing with many different
// callbacks types. ScopedWebCallbacks provides a generic and relatively
// lightweight solution to this problem.
//
// Example usage:
//
//   using FooCallbacks = blink::WebCallbacks<const Foo&, const FooError&>;
//
//   void RespondWithSuccess(ScopedWebCallbacks<FooCallbacks> callbacks) {
//     callbacks.PassCallbacks()->onSuccess(Foo("everything is great"));
//   }
//
//   void OnCallbacksDropped(std::unique_ptr<FooCallbacks> callbacks) {
//     // Ownership of the FooCallbacks is passed to this function if
//     // ScopedWebCallbacks::PassCallbacks isn't called before the
//     // ScopedWebCallbacks is destroyed.
//     callbacks->onError(FooError("everything is terrible"));
//   }
//
//   // Blink client implementation
//   void FooClientImpl::doMagic(std::unique_ptr<FooCallbacks> callbacks) {
//     auto scoped_callbacks = make_scoped_web_callbacks(
//         std::move(callbacks), WTF::BindOnce(&OnCallbacksDropped));
//
//     // Call to some lower-level service which may never run the callback we
//     // give it.
//     foo_service_->DoMagic(WTF::BindOnce(&RespondWithSuccess,
//                                          std::move(scoped_callbacks)));
//   }
//
// If the bound RespondWithSuccess callback actually runs, PassCallbacks() will
// reliquish ownership of the WebCallbacks object to a temporary scoped_ptr
// which will be destroyed immediately after onSuccess is called.
//
// If the bound RespondWithSuccess callback is instead destroyed first,
// the ScopedWebCallbacks destructor will invoke OnCallbacksDropped, executing
// our desired default behavior before deleting the WebCallbacks.
template <typename CallbacksType>
class ScopedWebCallbacks {
 public:
  using DestructionCallback =
      base::OnceCallback<void(std::unique_ptr<CallbacksType> callbacks)>;

  ScopedWebCallbacks(std::unique_ptr<CallbacksType> callbacks,
                     DestructionCallback destruction_callback)
      : callbacks_(std::move(callbacks)),
        destruction_callback_(std::move(destruction_callback)) {}

  ~ScopedWebCallbacks() {
    if (destruction_callback_)
      std::move(destruction_callback_).Run(std::move(callbacks_));
  }

  ScopedWebCallbacks(ScopedWebCallbacks&& other) = default;
  ScopedWebCallbacks(const ScopedWebCallbacks& other) = delete;

  ScopedWebCallbacks& operator=(ScopedWebCallbacks&& other) = default;
  ScopedWebCallbacks& operator=(const ScopedWebCallbacks& other) = delete;

  std::unique_ptr<CallbacksType> PassCallbacks() {
    destruction_callback_ = DestructionCallback();
    return std::move(callbacks_);
  }

 private:
  std::unique_ptr<CallbacksType> callbacks_;
  DestructionCallback destruction_callback_;
};

template <typename CallbacksType>
ScopedWebCallbacks<CallbacksType> MakeScopedWebCallbacks(
    std::unique_ptr<CallbacksType> callbacks,
    typename ScopedWebCallbacks<CallbacksType>::DestructionCallback
        destruction_callback) {
  return ScopedWebCallbacks<CallbacksType>(std::move(callbacks),
                                           std::move(destruction_callback));
}

using ImageCaptureGrabFrameCallbacks =
    CallbackPromiseAdapter<ImageBitmap, void>;

// This class grabs Video Frames from a given Media Stream Video Track, binding
// a method of an ephemeral SingleShotFrameHandler every time grabFrame() is
// called. This method receives an incoming media::VideoFrame on a background
// thread and converts it into the appropriate SkBitmap which is sent back to
// OnSkBitmap(). This class is single threaded throughout.
class ImageCaptureFrameGrabber final : public MediaStreamVideoSink {
 public:
  ImageCaptureFrameGrabber() = default;

  ImageCaptureFrameGrabber(const ImageCaptureFrameGrabber&) = delete;
  ImageCaptureFrameGrabber& operator=(const ImageCaptureFrameGrabber&) = delete;

  ~ImageCaptureFrameGrabber() override;

  void GrabFrame(MediaStreamComponent* component,
                 std::unique_ptr<ImageCaptureGrabFrameCallbacks> callbacks,
                 scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                 base::TimeDelta timeout);

 private:
  // Internal class to receive, convert and forward one frame.
  class SingleShotFrameHandler;

  void OnSkImage(ScopedWebCallbacks<ImageCaptureGrabFrameCallbacks> callbacks,
                 sk_sp<SkImage> image);
  void OnTimeout();

  // Flag to indicate that there is a frame grabbing in progress.
  bool frame_grab_in_progress_ = false;
  TaskHandle timeout_task_handle_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<ImageCaptureFrameGrabber> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGECAPTURE_IMAGE_CAPTURE_FRAME_GRABBER_H_
