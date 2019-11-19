// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FRAME_REQUEST_CALLBACK_COLLECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FRAME_REQUEST_CALLBACK_COLLECTION_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_frame_request_callback.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/probe/async_task_id.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ExecutionContext;

class GC_PLUGIN_IGNORE("crbug.com/841830")
    CORE_EXPORT FrameRequestCallbackCollection final : public NameClient {
  DISALLOW_NEW();

 public:
  explicit FrameRequestCallbackCollection(ExecutionContext*);

  using CallbackId = int;

  // |FrameCallback| is an interface type which generalizes callbacks which are
  // invoked when a script-based animation needs to be resampled.
  class CORE_EXPORT FrameCallback : public GarbageCollected<FrameCallback>,
                                    public NameClient {
   public:
    virtual void Trace(Visitor* visitor) {}
    const char* NameInHeapSnapshot() const override { return "FrameCallback"; }
    virtual ~FrameCallback() = default;
    virtual void Invoke(double) = 0;

    int Id() const { return id_; }
    bool IsCancelled() const { return is_cancelled_; }
    bool GetUseLegacyTimeBase() const { return use_legacy_time_base_; }
    void SetId(int id) { id_ = id; }
    void SetIsCancelled(bool is_cancelled) { is_cancelled_ = is_cancelled; }
    void SetUseLegacyTimeBase(bool use_legacy_time_base) {
      use_legacy_time_base_ = use_legacy_time_base;
    }

    probe::AsyncTaskId* async_task_id() { return &async_task_id_; }

   protected:
    FrameCallback() = default;

   private:
    int id_ = 0;
    bool is_cancelled_ = false;
    bool use_legacy_time_base_ = false;
    probe::AsyncTaskId async_task_id_;
  };

  // |V8FrameCallback| is an adapter class for the conversion from
  // |V8FrameRequestCallback| to |Framecallback|.
  class CORE_EXPORT V8FrameCallback : public FrameCallback {
   public:
    void Trace(Visitor*) override;
    const char* NameInHeapSnapshot() const override {
      return "V8FrameCallback";
    }

    explicit V8FrameCallback(V8FrameRequestCallback*);
    ~V8FrameCallback() override = default;

    void Invoke(double) override;

   private:
    Member<V8FrameRequestCallback> callback_;
  };

  CallbackId RegisterFrameCallback(FrameCallback*);
  void CancelFrameCallback(CallbackId);
  void ExecuteFrameCallbacks(double high_res_now_ms,
                             double high_res_now_ms_legacy);

  CallbackId RegisterPostFrameCallback(FrameCallback*);
  void CancelPostFrameCallback(CallbackId);
  void ExecutePostFrameCallbacks(double high_res_now_ms,
                                 double high_rest_now_ms_legacy);

  bool HasFrameCallback() const { return frame_callbacks_.size(); }
  bool HasPostFrameCallback() const { return post_frame_callbacks_.size(); }
  bool IsEmpty() const {
    return !HasFrameCallback() && !HasPostFrameCallback();
  }

  void Trace(Visitor*);
  const char* NameInHeapSnapshot() const override {
    return "FrameRequestCallbackCollection";
  }

 private:
  using CallbackList = HeapVector<Member<FrameCallback>>;
  void ExecuteCallbacksInternal(CallbackList& callbacks,
                                const char* trace_event_name,
                                const char* probe_name,
                                double high_res_now_ms,
                                double high_res_now_ms_legacy);
  void CancelCallbackInternal(CallbackId id,
                              const char* trace_event_name,
                              const char* probe_name);

  CallbackList frame_callbacks_;
  CallbackList post_frame_callbacks_;
  // only non-empty while inside ExecuteCallbacks or ExecutePostFrameCallbacks.
  CallbackList callbacks_to_invoke_;

  CallbackId next_callback_id_ = 0;

  Member<ExecutionContext> context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FRAME_REQUEST_CALLBACK_COLLECTION_H_
