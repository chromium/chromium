// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transport.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace WTF {
template <>
struct CrossThreadCopier<Vector<scoped_refptr<blink::FeedbackProvider>>>
    : public CrossThreadCopierPassThrough<
          Vector<scoped_refptr<blink::FeedbackProvider>>> {
  STATIC_ONLY(CrossThreadCopier);
};
}  // namespace WTF

namespace blink {

// This method runs in the worker context, once PostCustomEvent appears.
Event* CreateEvent(
    CrossThreadWeakHandle<RTCRtpTransport> rtp_transport,
    scoped_refptr<base::SequencedTaskRunner> rtp_transport_task_runner,
    ScriptState* script_state,
    CustomEventMessage data) {
  auto* processor = MakeGarbageCollected<RTCRtpTransportProcessor>(
      ExecutionContext::From(script_state));
  auto* event = MakeGarbageCollected<RTCRtpTransportProcessorEvent>(processor);

  // Reply to the RTCRtpTransport object on the main thread with a handle to
  // the created Processor.
  PostCrossThreadTask(
      *rtp_transport_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &RTCRtpTransport::SetProcessorHandle,
          MakeUnwrappingCrossThreadWeakHandle(rtp_transport),
          MakeCrossThreadWeakHandle(processor),
          WrapRefCounted(ExecutionContext::From(script_state)
                             ->GetTaskRunner(TaskType::kInternalMediaRealTime)
                             .get())));
  return event;
}

RTCRtpTransport::RTCRtpTransport(ExecutionContext* context)
    : ExecutionContextClient(context) {}

RTCRtpTransport::~RTCRtpTransport() = default;

void RTCRtpTransport::createProcessor(ScriptState* script_state,
                                      DedicatedWorker* worker,
                                      ExceptionState& exception_state) {
  createProcessor(script_state, worker, ScriptValue(), exception_state);
}

void RTCRtpTransport::createProcessor(ScriptState* script_state,
                                      DedicatedWorker* worker,
                                      const ScriptValue& options,
                                      ExceptionState& exception_state) {
  HeapVector<ScriptValue> transfer;
  createProcessor(script_state, worker, options, transfer, exception_state);
}

void RTCRtpTransport::createProcessor(ScriptState* script_state,
                                      DedicatedWorker* worker,
                                      const ScriptValue& options,
                                      HeapVector<ScriptValue>& transfer,
                                      ExceptionState& exception_state) {
  worker->PostCustomEvent(
      TaskType::kInternalMediaRealTime, script_state,
      CrossThreadBindRepeating(
          &CreateEvent, MakeCrossThreadWeakHandle(this),
          ExecutionContext::From(script_state)
              ->GetTaskRunner(TaskType::kInternalMediaRealTime)),
      CrossThreadFunction<Event*(ScriptState*)>(), options, transfer,
      exception_state);
}

void RTCRtpTransport::RegisterFeedbackProvider(
    scoped_refptr<FeedbackProvider> feedback_provider) {
  if (processor_) {
    CHECK(processor_task_runner_);
    feedback_provider->SetProcessor(*processor_, processor_task_runner_);
  }

  feedback_providers_.push_back(std::move(feedback_provider));

  if (processor_) {
    PostCrossThreadTask(
        *processor_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RTCRtpTransportProcessor ::SetFeedbackProviders,
                            MakeUnwrappingCrossThreadWeakHandle(*processor_),
                            feedback_providers_));
  }
}

void RTCRtpTransport::SetProcessorHandle(
    CrossThreadWeakHandle<RTCRtpTransportProcessor> processor,
    scoped_refptr<base::SequencedTaskRunner> processor_task_runner) {
  processor_.emplace(std::move(processor));
  processor_task_runner_ = processor_task_runner;

  for (auto& feedback_provider : feedback_providers_) {
    feedback_provider->SetProcessor(*processor_, processor_task_runner_);
  }

  PostCrossThreadTask(
      *processor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&RTCRtpTransportProcessor ::SetFeedbackProviders,
                          MakeUnwrappingCrossThreadWeakHandle(*processor_),
                          feedback_providers_));
}

void RTCRtpTransport::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
