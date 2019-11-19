// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "third_party/blink/renderer/modules/sms/sms_receiver.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/sms/sms.h"
#include "third_party/blink/renderer/modules/sms/sms_metrics.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

SMSReceiver::SMSReceiver(ExecutionContext* context) : ContextClient(context) {}

ScriptPromise SMSReceiver::receive(ScriptState* script_state,
                                   const SMSReceiverOptions* options) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  LocalFrame* frame = GetFrame();
  if (!frame->IsMainFrame() && frame->IsCrossOriginSubframe()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kNotAllowedError,
                          "Must have the same origin as the top-level frame."));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  requests_.insert(resolver);

  // See https://bit.ly/2S0zRAS for task types.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);

  if (!service_) {
    GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver(task_runner));
    service_.set_disconnect_handler(WTF::Bind(
        &SMSReceiver::OnSMSReceiverConnectionError, WrapWeakPersistent(this)));
  }

  service_->Receive(
      WTF::Bind(&SMSReceiver::OnReceive, WrapPersistent(this),
                WrapPersistent(resolver), base::TimeTicks::Now()));

  return resolver->Promise();
}

SMSReceiver::~SMSReceiver() = default;

void SMSReceiver::OnReceive(ScriptPromiseResolver* resolver,
                            base::TimeTicks start_time,
                            mojom::blink::SmsStatus status,
                            const WTF::String& sms) {
  requests_.erase(resolver);

  ukm::SourceId source_id = GetDocument()->UkmSourceID();
  ukm::UkmRecorder* recorder = GetDocument()->UkmRecorder();

  if (status == mojom::blink::SmsStatus::kTimeout) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kTimeoutError, "SMSReceiver timed out."));
    RecordSMSOutcome(SMSReceiverOutcome::kTimeout, source_id, recorder);
    return;
  } else if (status == mojom::blink::SmsStatus::kCancelled) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kAbortError, "SMSReceiver was aborted."));
    RecordSMSCancelTime(base::TimeTicks::Now() - start_time);
    RecordSMSOutcome(SMSReceiverOutcome::kCancelled, source_id, recorder);
    return;
  }

  RecordSMSSuccessTime(base::TimeTicks::Now() - start_time);
  RecordSMSOutcome(SMSReceiverOutcome::kSuccess, source_id, recorder);

  resolver->Resolve(MakeGarbageCollected<blink::SMS>(sms));
}

void SMSReceiver::OnSMSReceiverConnectionError() {
  service_.reset();
  for (ScriptPromiseResolver* request : requests_) {
    request->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, "SMSReceiver not available."));
    RecordSMSOutcome(SMSReceiverOutcome::kConnectionError,
                     GetDocument()->UkmSourceID(),
                     GetDocument()->UkmRecorder());
  }
  requests_.clear();
}

void SMSReceiver::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  ContextClient::Trace(visitor);
  visitor->Trace(requests_);
}

}  // namespace blink
