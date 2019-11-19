// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

FrameRequestCallbackCollection::FrameRequestCallbackCollection(
    ExecutionContext* context)
    : context_(context) {}

FrameRequestCallbackCollection::CallbackId
FrameRequestCallbackCollection::RegisterFrameCallback(FrameCallback* callback) {
  FrameRequestCallbackCollection::CallbackId id = ++next_callback_id_;
  callback->SetIsCancelled(false);
  callback->SetId(id);
  frame_callbacks_.push_back(callback);

  TRACE_EVENT_INSTANT1("devtools.timeline", "RequestAnimationFrame",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_animation_frame_event::Data(context_, id));
  probe::AsyncTaskScheduledBreakable(context_, "requestAnimationFrame",
                                     callback->async_task_id());
  return id;
}

void FrameRequestCallbackCollection::CancelFrameCallback(CallbackId id) {
  CancelCallbackInternal(id, "CancelAnimationFrame", "cancelAnimationFrame");
}

void FrameRequestCallbackCollection::CancelPostFrameCallback(CallbackId id) {
  CancelCallbackInternal(id, "CancelPostAnimationFrame",
                         "cancelPostAnimationFrame");
}

void FrameRequestCallbackCollection::CancelCallbackInternal(
    CallbackId id,
    const char* trace_event_name,
    const char* probe_name) {
  for (wtf_size_t i = 0; i < frame_callbacks_.size(); ++i) {
    if (frame_callbacks_[i]->Id() == id) {
      probe::AsyncTaskCanceledBreakable(context_, probe_name,
                                        frame_callbacks_[i]->async_task_id());
      frame_callbacks_.EraseAt(i);
      TRACE_EVENT_INSTANT1("devtools.timeline", trace_event_name,
                           TRACE_EVENT_SCOPE_THREAD, "data",
                           inspector_animation_frame_event::Data(context_, id));
      return;
    }
  }
  for (wtf_size_t i = 0; i < post_frame_callbacks_.size(); ++i) {
    if (post_frame_callbacks_[i]->Id() == id) {
      probe::AsyncTaskCanceledBreakable(
          context_, probe_name, post_frame_callbacks_[i]->async_task_id());
      post_frame_callbacks_.EraseAt(i);
      TRACE_EVENT_INSTANT1("devtools.timeline", trace_event_name,
                           TRACE_EVENT_SCOPE_THREAD, "data",
                           inspector_animation_frame_event::Data(context_, id));
      return;
    }
  }
  for (const auto& callback : callbacks_to_invoke_) {
    if (callback->Id() == id) {
      probe::AsyncTaskCanceledBreakable(context_, probe_name,
                                        callback->async_task_id());
      TRACE_EVENT_INSTANT1("devtools.timeline", trace_event_name,
                           TRACE_EVENT_SCOPE_THREAD, "data",
                           inspector_animation_frame_event::Data(context_, id));
      callback->SetIsCancelled(true);
      // will be removed at the end of ExecuteCallbacks() or
      // ExecutePostFrameCallbacks()
      return;
    }
  }
}

void FrameRequestCallbackCollection::ExecuteFrameCallbacks(
    double high_res_now_ms,
    double high_res_now_ms_legacy) {
  TRACE_EVENT0("blink",
               "FrameRequestCallbackCollection::ExecuteFrameCallbacks");
  ExecuteCallbacksInternal(frame_callbacks_, "FireAnimationFrame",
                           "requestAnimationFrame", high_res_now_ms,
                           high_res_now_ms_legacy);
}

void FrameRequestCallbackCollection::ExecutePostFrameCallbacks(
    double high_res_now_ms,
    double high_res_now_ms_legacy) {
  ExecuteCallbacksInternal(post_frame_callbacks_, "FirePostAnimationFrame",
                           "requestPostAnimationFrame", high_res_now_ms,
                           high_res_now_ms_legacy);
}

void FrameRequestCallbackCollection::ExecuteCallbacksInternal(
    CallbackList& callbacks,
    const char* trace_event_name,
    const char* probe_name,
    double high_res_now_ms,
    double high_res_now_ms_legacy) {
  // First, generate a list of callbacks to consider.  Callbacks registered from
  // this point on are considered only for the "next" frame, not this one.
  DCHECK(callbacks_to_invoke_.IsEmpty());
  swap(callbacks_to_invoke_, callbacks);

  for (const auto& callback : callbacks_to_invoke_) {
    // When the ExecutionContext is destroyed (e.g. an iframe is detached),
    // there is no path to perform wrapper tracing for the callbacks. In such a
    // case, the callback functions may already have been collected by V8 GC.
    // Since it's possible that a callback function being invoked detaches an
    // iframe, we need to check the condition for each callback.
    if (context_->IsContextDestroyed())
      break;
    if (callback->IsCancelled()) {
      // Another requestAnimationFrame callback already cancelled this one
      UseCounter::Count(context_,
                        WebFeature::kAnimationFrameCancelledWithinFrame);
      continue;
    }
    TRACE_EVENT1(
        "devtools.timeline", trace_event_name, "data",
        inspector_animation_frame_event::Data(context_, callback->Id()));
    probe::AsyncTask async_task(context_, callback->async_task_id());
    probe::UserCallback probe(context_, probe_name, AtomicString(), true);
    if (callback->GetUseLegacyTimeBase())
      callback->Invoke(high_res_now_ms_legacy);
    else
      callback->Invoke(high_res_now_ms);
  }

  callbacks_to_invoke_.clear();
}

FrameRequestCallbackCollection::CallbackId
FrameRequestCallbackCollection::RegisterPostFrameCallback(
    FrameCallback* callback) {
  CallbackId id = ++next_callback_id_;
  callback->SetIsCancelled(false);
  callback->SetId(id);
  post_frame_callbacks_.push_back(callback);

  TRACE_EVENT_INSTANT1("devtools.timeline", "RequestPostAnimationFrame",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_animation_frame_event::Data(context_, id));
  probe::AsyncTaskScheduledBreakable(context_, "requestPostAnimationFrame",
                                     callback->async_task_id());
  return id;
}

void FrameRequestCallbackCollection::Trace(Visitor* visitor) {
  visitor->Trace(frame_callbacks_);
  visitor->Trace(post_frame_callbacks_);
  visitor->Trace(callbacks_to_invoke_);
  visitor->Trace(context_);
}

FrameRequestCallbackCollection::V8FrameCallback::V8FrameCallback(
    V8FrameRequestCallback* callback)
    : callback_(callback) {}

void FrameRequestCallbackCollection::V8FrameCallback::Trace(
    blink::Visitor* visitor) {
  visitor->Trace(callback_);
  FrameRequestCallbackCollection::FrameCallback::Trace(visitor);
}

void FrameRequestCallbackCollection::V8FrameCallback::Invoke(
    double highResTime) {
  callback_->InvokeAndReportException(nullptr, highResTime);
}

}  // namespace blink
