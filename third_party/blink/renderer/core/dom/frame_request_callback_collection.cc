// Copyright 2015 The Chromium Authors
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
FrameRequestCallbackCollection::RegisterFrameCallback(FrameCallback* callback,
                                                      FrameCallbackType type) {
  FrameRequestCallbackCollection::CallbackId id =
      (type == FrameCallbackType::kInternal) ? ++next_internal_callback_id_
                                             : ++next_callback_id_;
  callback->SetIsCancelled(false);
  callback->SetId(id);
  if (type == FrameCallbackType::kInternal) {
    internal_frame_callbacks_.push_back(callback);
  } else {
    frame_callbacks_.push_back(callback);
  }
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT("RequestAnimationFrame",
                                        inspector_animation_frame_event::Data,
                                        context_, id);
  callback->async_task_context()->Schedule(context_, "requestAnimationFrame");
  probe::BreakableLocation(context_, "requestAnimationFrame");
  return id;
}

void FrameRequestCallbackCollection::CancelFrameCallback(
    CallbackId id,
    FrameCallbackType type) {
  auto& callbacks = (type == FrameCallbackType::kInternal)
                        ? internal_frame_callbacks_
                        : frame_callbacks_;
  auto& callbacks_to_invoke = (type == FrameCallbackType::kInternal)
                                  ? internal_callbacks_to_invoke_
                                  : callbacks_to_invoke_;

  for (wtf_size_t i = 0; i < callbacks.size(); ++i) {
    if (callbacks[i]->Id() == id) {
      callbacks[i]->async_task_context()->Cancel();
      probe::BreakableLocation(context_, "cancelAnimationFrame");
      callbacks.EraseAt(i);
      DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
          "CancelAnimationFrame", inspector_animation_frame_event::Data,
          context_.Get(), id);
      return;
    }
  }
  for (const auto& callback : callbacks_to_invoke) {
    if (callback->Id() == id) {
      callback->async_task_context()->Cancel();
      probe::BreakableLocation(context_, "cancelAnimationFrame");
      DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
          "CancelAnimationFrame", inspector_animation_frame_event::Data,
          context_.Get(), id);
      callback->SetIsCancelled(true);
      // will be removed at the end of ExecuteCallbacks()
      return;
    }
  }
}

void FrameRequestCallbackCollection::ExecuteFrameCallbacks(
    double high_res_now_ms,
    double high_res_now_ms_legacy) {
  TRACE_EVENT0("blink",
               "FrameRequestCallbackCollection::ExecuteFrameCallbacks");
  ExecutionContext::ScopedRequestAnimationFrameStatus scoped_raf_status(
      context_);

  // First, snapshot both lists. Callbacks registered from this point on are
  // considered only for the "next" frame, not this one.
  DCHECK(callbacks_to_invoke_.empty());
  DCHECK(internal_callbacks_to_invoke_.empty());
  swap(callbacks_to_invoke_, frame_callbacks_);
  swap(internal_callbacks_to_invoke_, internal_frame_callbacks_);

  ExecuteFrameCallbacksImpl(callbacks_to_invoke_, high_res_now_ms,
                            high_res_now_ms_legacy);
  ExecuteFrameCallbacksImpl(internal_callbacks_to_invoke_, high_res_now_ms,
                            high_res_now_ms_legacy);
}

void FrameRequestCallbackCollection::ExecuteFrameCallbacksImpl(
    CallbackList& callbacks_to_invoke,
    double high_res_now_ms,
    double high_res_now_ms_legacy) {
  for (const auto& callback : callbacks_to_invoke) {
    // When the ExecutionContext is destroyed (e.g. an iframe is detached),
    // there is no path to perform wrapper tracing for the callbacks. In such a
    // case, the callback functions may already have been collected by V8 GC.
    // Since it's possible that a callback function being invoked detaches an
    // iframe, we need to check the condition for each callback.
    if (context_->IsContextDestroyed()) {
      break;
    }
    if (callback->IsCancelled()) {
      // Another requestAnimationFrame callback already cancelled this one
      UseCounter::Count(context_,
                        WebFeature::kAnimationFrameCancelledWithinFrame);
      continue;
    }
    DEVTOOLS_TIMELINE_TRACE_EVENT("FireAnimationFrame",
                                  inspector_animation_frame_event::Data,
                                  context_, callback->Id());
    probe::AsyncTask async_task(context_, callback->async_task_context());
    probe::UserCallback probe(context_, "requestAnimationFrame", AtomicString(),
                              true);
    if (callback->GetUseLegacyTimeBase()) {
      callback->Invoke(high_res_now_ms_legacy);
    } else {
      callback->Invoke(high_res_now_ms);
    }
  }

  callbacks_to_invoke.clear();
}

void FrameRequestCallbackCollection::Trace(Visitor* visitor) const {
  visitor->Trace(frame_callbacks_);
  visitor->Trace(callbacks_to_invoke_);
  visitor->Trace(internal_frame_callbacks_);
  visitor->Trace(internal_callbacks_to_invoke_);
  visitor->Trace(context_);
}

V8FrameCallback::V8FrameCallback(V8FrameRequestCallback* callback)
    : callback_(callback) {}

void V8FrameCallback::Trace(blink::Visitor* visitor) const {
  visitor->Trace(callback_);
  FrameCallback::Trace(visitor);
}

void V8FrameCallback::Invoke(double highResTime) {
  callback_->InvokeAndReportException(nullptr, highResTime);
}

}  // namespace blink
