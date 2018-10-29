/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"

#include <memory>
#include "third_party/blink/renderer/bindings/core/v8/serialization/post_message_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger.h"
#include "third_party/blink/renderer/core/messaging/post_message_options.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_object_proxy.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_thread.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_module_tree_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"

namespace blink {

DedicatedWorkerGlobalScope::DedicatedWorkerGlobalScope(
    const String& name,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    DedicatedWorkerThread* thread,
    base::TimeTicks time_origin)
    : WorkerGlobalScope(std::move(creation_params), thread, time_origin),
      name_(name) {}

DedicatedWorkerGlobalScope::~DedicatedWorkerGlobalScope() = default;

const AtomicString& DedicatedWorkerGlobalScope::InterfaceName() const {
  return EventTargetNames::DedicatedWorkerGlobalScope;
}

// https://html.spec.whatwg.org/multipage/workers.html#worker-processing-model
void DedicatedWorkerGlobalScope::ImportModuleScript(
    const KURL& module_url_record,
    FetchClientSettingsObjectSnapshot* outside_settings_object,
    network::mojom::FetchCredentialsMode credentials_mode) {
  // Step 12: "Let destination be "sharedworker" if is shared is true, and
  // "worker" otherwise."
  mojom::RequestContextType destination = mojom::RequestContextType::WORKER;

  Modulator* modulator = Modulator::From(ScriptController()->GetScriptState());

  // Step 13: "... Fetch a module worker script graph given url, outside
  // settings, destination, the value of the credentials member of options, and
  // inside settings."
  FetchModuleScript(module_url_record, outside_settings_object, destination,
                    credentials_mode,
                    ModuleScriptCustomFetchType::kWorkerConstructor,
                    new WorkerModuleTreeClient(modulator));
}

const String DedicatedWorkerGlobalScope::name() const {
  return name_;
}

void DedicatedWorkerGlobalScope::postMessage(ScriptState* script_state,
                                             const ScriptValue& message,
                                             Vector<ScriptValue>& transfer,
                                             ExceptionState& exception_state) {
  PostMessageOptions options;
  if (!transfer.IsEmpty())
    options.setTransfer(transfer);
  postMessage(script_state, message, options, exception_state);
}

void DedicatedWorkerGlobalScope::postMessage(ScriptState* script_state,
                                             const ScriptValue& message,
                                             const PostMessageOptions& options,
                                             ExceptionState& exception_state) {
  Transferables transferables;
  scoped_refptr<SerializedScriptValue> serialized_message =
      PostMessageHelper::SerializeMessageByMove(script_state->GetIsolate(),
                                                message, options, transferables,
                                                exception_state);
  if (exception_state.HadException())
    return;
  DCHECK(serialized_message);
  BlinkTransferableMessage transferable_message;
  transferable_message.message = serialized_message;
  // Disentangle the port in preparation for sending it to the remote context.
  transferable_message.ports = MessagePort::DisentanglePorts(
      ExecutionContext::From(script_state), transferables.message_ports,
      exception_state);
  if (exception_state.HadException())
    return;
  ThreadDebugger* debugger = ThreadDebugger::From(script_state->GetIsolate());
  transferable_message.sender_stack_trace_id =
      debugger->StoreCurrentStackTrace("postMessage");
  WorkerObjectProxy().PostMessageToWorkerObject(
      std::move(transferable_message));
}

DedicatedWorkerObjectProxy& DedicatedWorkerGlobalScope::WorkerObjectProxy()
    const {
  return static_cast<DedicatedWorkerThread*>(GetThread())->WorkerObjectProxy();
}

void DedicatedWorkerGlobalScope::Trace(blink::Visitor* visitor) {
  WorkerGlobalScope::Trace(visitor);
}

}  // namespace blink
