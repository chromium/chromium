// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/video_rvfc/video_frame_request_callback_collection.h"

#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

VideoFrameRequestCallbackCollection::VideoFrameRequestCallbackCollection(
    ExecutionContext* context)
    : context_(context) {}

VideoFrameRequestCallbackCollection::CallbackId
VideoFrameRequestCallbackCollection::RegisterFrameCallback(
    VideoFrameCallback* callback) {
  VideoFrameRequestCallbackCollection::CallbackId id = ++next_callback_id_;
  callback->SetIsCancelled(false);
  callback->SetId(id);
  frame_callbacks_.push_back(callback);

  return id;
}

void VideoFrameRequestCallbackCollection::CancelFrameCallback(CallbackId id) {
  for (wtf_size_t i = 0; i < frame_callbacks_.size(); ++i) {
    if (frame_callbacks_[i]->Id() == id) {
      frame_callbacks_.EraseAt(i);
      return;
    }
  }
  for (const auto& callback : callbacks_to_invoke_) {
    if (callback->Id() == id) {
      callback->SetIsCancelled(true);
      // will be removed at the end of ExecuteCallbacks().
      return;
    }
  }
}

void VideoFrameRequestCallbackCollection::ExecuteFrameCallbacks(
    double high_res_now_ms,
    const VideoFrameCallbackMetadata* metadata) {
  // First, generate a list of callbacks to consider. Callbacks registered from
  // this point on are considered only for the "next" frame, not this one.
  DCHECK(callbacks_to_invoke_.empty());
  std::swap(callbacks_to_invoke_, frame_callbacks_);

  for (const auto& callback : callbacks_to_invoke_) {
    // When the ExecutionContext is destroyed (e.g. an iframe is detached),
    // there is no path to perform wrapper tracing for the callbacks. In such a
    // case, the callback functions may already have been collected by V8 GC.
    // Since it's possible that a callback function being invoked detaches an
    // iframe, we need to check the condition for each callback.
    if (context_->IsContextDestroyed())
      break;

    // Another requestAnimationFrame callback already cancelled this one.
    if (callback->IsCancelled())
      continue;

    callback->Invoke(high_res_now_ms, metadata);
  }

  callbacks_to_invoke_.clear();
}

void VideoFrameRequestCallbackCollection::Trace(Visitor* visitor) const {
  visitor->Trace(frame_callbacks_);
  visitor->Trace(callbacks_to_invoke_);
  visitor->Trace(context_);
}

VideoFrameRequestCallbackCollection::V8VideoFrameCallback::V8VideoFrameCallback(
    V8VideoFrameRequestCallback* callback)
    : callback_(callback) {}

void VideoFrameRequestCallbackCollection::V8VideoFrameCallback::Trace(
    blink::Visitor* visitor) const {
  visitor->Trace(callback_);
  VideoFrameRequestCallbackCollection::VideoFrameCallback::Trace(visitor);
}

void VideoFrameRequestCallbackCollection::V8VideoFrameCallback::Invoke(
    double highResTime,
    const VideoFrameCallbackMetadata* metadata) {
  callback_->InvokeAndReportException(nullptr, highResTime, metadata);
}

}  // namespace blink
