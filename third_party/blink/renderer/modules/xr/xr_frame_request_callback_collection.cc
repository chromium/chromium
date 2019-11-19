// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_frame_request_callback_collection.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_frame_request_callback.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRFrameRequestCallbackCollection::XRFrameRequestCallbackCollection(
    ExecutionContext* context)
    : context_(context) {}

XRFrameRequestCallbackCollection::CallbackId
XRFrameRequestCallbackCollection::RegisterCallback(
    V8XRFrameRequestCallback* callback) {
  CallbackId id = ++next_callback_id_;
  auto add_result =
      callbacks_.Set(id, CallbackAndAsyncTask(callback, probe::AsyncTaskId()));
  pending_callbacks_.push_back(id);

  probe::AsyncTaskScheduledBreakable(context_, "XRRequestFrame",
                                     &add_result.stored_value->value.second);
  return id;
}

void XRFrameRequestCallbackCollection::CancelCallback(CallbackId id) {
  if (IsValidCallbackId(id)) {
    callbacks_.erase(id);
    current_callbacks_.erase(id);
  }
}

void XRFrameRequestCallbackCollection::ExecuteCallbacks(XRSession* session,
                                                        double timestamp,
                                                        XRFrame* frame) {
  // First, generate a list of callbacks to consider.  Callbacks registered from
  // this point on are considered only for the "next" frame, not this one.

  // Conceptually we are just going to iterate through current_callbacks_, and
  // call each callback.  However, if we had multiple callbacks, subsequent ones
  // could be removed while we are iterating.  HeapHashMap iterators aren't
  // valid after collection modifications, so we also store a corresponding set
  // of ids for iteration purposes.  current_callback_ids is the set of ids for
  // callbacks we will call, and is kept in sync with current_callbacks_ but
  // safe to iterate over.
  DCHECK(current_callbacks_.IsEmpty());
  current_callbacks_.swap(callbacks_);

  Vector<CallbackId> current_callback_ids;
  current_callback_ids.swap(pending_callbacks_);

  for (const auto& id : current_callback_ids) {
    auto it = current_callbacks_.find(id);
    if (it == current_callbacks_.end())
      continue;

    probe::AsyncTask async_task(context_, &it->value.second);
    probe::UserCallback probe(context_, "XRRequestFrame", AtomicString(), true);
    it->value.first->InvokeAndReportException(session, timestamp, frame);
  }

  current_callbacks_.clear();
}

void XRFrameRequestCallbackCollection::Trace(blink::Visitor* visitor) {
  visitor->Trace(callbacks_);
  visitor->Trace(current_callbacks_);
  visitor->Trace(context_);
}

}  // namespace blink
