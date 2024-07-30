// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_frame_request_callback_collection.h"

#include "base/not_fatal_until.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_frame_request_callback.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/async_task_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

XRFrameRequestCallbackCollection::XRFrameRequestCallbackCollection(
    ExecutionContext* context)
    : context_(context) {}

XRFrameRequestCallbackCollection::CallbackId
XRFrameRequestCallbackCollection::RegisterCallback(
    V8XRFrameRequestCallback* callback) {
  CallbackId id = ++next_callback_id_;
  auto add_result_frame_request = callback_frame_requests_.Set(id, callback);
  auto add_result_async_task = callback_async_tasks_.Set(
      id, std::make_unique<probe::AsyncTaskContext>());
  DCHECK_EQ(add_result_frame_request.is_new_entry,
            add_result_async_task.is_new_entry);
  pending_callbacks_.push_back(id);

  add_result_async_task.stored_value->value->Schedule(context_,
                                                      "XRRequestFrame");
  probe::BreakableLocation(context_, "XRRequestFrame");
  return id;
}

void XRFrameRequestCallbackCollection::CancelCallback(CallbackId id) {
  if (IsValidCallbackId(id)) {
    callback_frame_requests_.erase(id);
    callback_async_tasks_.erase(id);
    current_callback_frame_requests_.erase(id);
    current_callback_async_tasks_.erase(id);
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
  DCHECK(current_callback_frame_requests_.empty());
  DCHECK(current_callback_async_tasks_.empty());
  current_callback_frame_requests_.swap(callback_frame_requests_);
  current_callback_async_tasks_.swap(callback_async_tasks_);

  Vector<CallbackId> current_callback_ids;
  current_callback_ids.swap(pending_callbacks_);

  for (const auto& id : current_callback_ids) {
    auto it_frame_request = current_callback_frame_requests_.find(id);
    auto it_async_task = current_callback_async_tasks_.find(id);
    if (it_frame_request == current_callback_frame_requests_.end()) {
      DCHECK_EQ(current_callback_async_tasks_.end(), it_async_task);
      continue;
    }
    CHECK_NE(current_callback_async_tasks_.end(), it_async_task,
             base::NotFatalUntil::M130);

    probe::AsyncTask async_task(context_, it_async_task->value.get());
    it_frame_request->value->InvokeAndReportException(session, timestamp,
                                                      frame);
  }

  current_callback_frame_requests_.clear();
  current_callback_async_tasks_.clear();
}

void XRFrameRequestCallbackCollection::Trace(Visitor* visitor) const {
  visitor->Trace(callback_frame_requests_);
  visitor->Trace(current_callback_frame_requests_);
  visitor->Trace(context_);
}

}  // namespace blink
