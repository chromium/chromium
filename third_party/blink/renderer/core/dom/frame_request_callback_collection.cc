// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"

#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

FrameRequestCallbackCollection::FrameRequestCallbackCollection(
    ExecutionContext* context)
    : context_(context) {}

FrameRequestCallbackCollection::CallbackId
FrameRequestCallbackCollection::RegisterCallback(FrameCallback* callback) {
  FrameRequestCallbackCollection::CallbackId id = ++next_callback_id_;
  callback->SetIsCancelled(false);
  callback->SetId(id);
  callbacks_.push_back(callback);

  TRACE_EVENT_INSTANT1("devtools.timeline", "RequestAnimationFrame",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorAnimationFrameEvent::Data(context_, id));
  probe::AsyncTaskScheduledBreakable(context_, "requestAnimationFrame",
                                     callback);
  return id;
}

void FrameRequestCallbackCollection::CancelCallback(CallbackId id) {
  for (wtf_size_t i = 0; i < callbacks_.size(); ++i) {
    if (callbacks_[i]->Id() == id) {
      probe::AsyncTaskCanceledBreakable(context_, "cancelAnimationFrame",
                                        callbacks_[i]);
      callbacks_.EraseAt(i);
      TRACE_EVENT_INSTANT1("devtools.timeline", "CancelAnimationFrame",
                           TRACE_EVENT_SCOPE_THREAD, "data",
                           InspectorAnimationFrameEvent::Data(context_, id));
      return;
    }
  }
  for (const auto& callback : callbacks_to_invoke_) {
    if (callback->Id() == id) {
      probe::AsyncTaskCanceledBreakable(context_, "cancelAnimationFrame",
                                        callback);
      TRACE_EVENT_INSTANT1("devtools.timeline", "CancelAnimationFrame",
                           TRACE_EVENT_SCOPE_THREAD, "data",
                           InspectorAnimationFrameEvent::Data(context_, id));
      callback->SetIsCancelled(true);
      // will be removed at the end of executeCallbacks()
      return;
    }
  }
}

void FrameRequestCallbackCollection::ExecuteCallbacks(
    double high_res_now_ms,
    double high_res_now_ms_legacy) {
  // First, generate a list of callbacks to consider.  Callbacks registered from
  // this point on are considered only for the "next" frame, not this one.
  DCHECK(callbacks_to_invoke_.IsEmpty());
  swap(callbacks_to_invoke_, callbacks_);

  for (const auto& callback : callbacks_to_invoke_) {
    // When the ExecutionContext is destroyed (e.g. an iframe is detached),
    // there is no path to perform wrapper tracing for the callbacks. In such a
    // case, the callback functions may already have been collected by V8 GC.
    // Since it's possible that a callback function being invoked detaches an
    // iframe, we need to check the condition for each callback.
    if (context_->IsContextDestroyed())
      break;

    if (!callback->IsCancelled()) {
      TRACE_EVENT1(
          "devtools.timeline", "FireAnimationFrame", "data",
          InspectorAnimationFrameEvent::Data(context_, callback->Id()));
      probe::AsyncTask async_task(context_, callback);
      probe::UserCallback probe(context_, "requestAnimationFrame",
                                AtomicString(), true);
      if (callback->GetUseLegacyTimeBase())
        callback->Invoke(high_res_now_ms_legacy);
      else
        callback->Invoke(high_res_now_ms);
    }
  }

  callbacks_to_invoke_.clear();
}

void FrameRequestCallbackCollection::Trace(blink::Visitor* visitor) {
  visitor->Trace(callbacks_);
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
