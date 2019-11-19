// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_REQUEST_CALLBACK_COLLECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_REQUEST_CALLBACK_COLLECTION_H_

#include "third_party/blink/renderer/core/probe/async_task_id.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ExecutionContext;
class V8XRFrameRequestCallback;
class XRFrame;
class XRSession;

class XRFrameRequestCallbackCollection final
    : public GarbageCollected<XRFrameRequestCallbackCollection>,
      public NameClient {
 public:
  explicit XRFrameRequestCallbackCollection(ExecutionContext*);

  using CallbackId = int;
  CallbackId RegisterCallback(V8XRFrameRequestCallback*);
  void CancelCallback(CallbackId);
  void ExecuteCallbacks(XRSession*, double timestamp, XRFrame*);

  bool IsEmpty() const { return !callbacks_.size(); }

  void Trace(blink::Visitor*);
  const char* NameInHeapSnapshot() const override {
    return "XRFrameRequestCallbackCollection";
  }

 private:
  bool IsValidCallbackId(int id) {
    using Traits = HashTraits<CallbackId>;
    return !Traits::IsDeletedValue(id) &&
           !WTF::IsHashTraitsEmptyValue<Traits, CallbackId>(id);
  }

  using CallbackAndAsyncTask =
      std::pair<Member<V8XRFrameRequestCallback>, probe::AsyncTaskId>;
  using CallbackMap = HeapHashMap<CallbackId, CallbackAndAsyncTask>;

  CallbackMap callbacks_;
  Vector<CallbackId> pending_callbacks_;

  // Only non-empty while inside executeCallbacks.
  CallbackMap current_callbacks_;

  CallbackId next_callback_id_ = 0;

  Member<ExecutionContext> context_;
};

}  // namespace blink

#endif  // FrameRequestCallbackCollection_h
