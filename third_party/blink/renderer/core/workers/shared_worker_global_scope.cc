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

#include "third_party/blink/renderer/core/workers/shared_worker_global_scope.h"

#include <memory>
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/workers/shared_worker_thread.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

SharedWorkerGlobalScope::SharedWorkerGlobalScope(
    const String& name,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    SharedWorkerThread* thread,
    base::TimeTicks time_origin)
    : WorkerGlobalScope(std::move(creation_params), thread, time_origin),
      name_(name) {}

SharedWorkerGlobalScope::~SharedWorkerGlobalScope() = default;

const AtomicString& SharedWorkerGlobalScope::InterfaceName() const {
  return EventTargetNames::SharedWorkerGlobalScope;
}

// https://html.spec.whatwg.org/multipage/workers.html#worker-processing-model
void SharedWorkerGlobalScope::ImportModuleScript(
    const KURL& module_url_record,
    FetchClientSettingsObjectSnapshot* outside_settings_object,
    network::mojom::FetchCredentialsMode credentials_mode) {
  // Step 12: "Let destination be "sharedworker" if is shared is true, and
  // "worker" otherwise."

  // Step 13: "... Fetch a module worker script graph given url, outside
  // settings, destination, the value of the credentials member of options, and
  // inside settings."

  // TODO(nhiroki): Implement module loading for shared workers.
  // (https://crbug.com/824646)
  NOTREACHED();
}

void SharedWorkerGlobalScope::ConnectPausable(MessagePortChannel channel) {
  if (IsContextPaused()) {
    AddPausedCall(WTF::Bind(&SharedWorkerGlobalScope::ConnectPausable,
                            WrapWeakPersistent(this), std::move(channel)));
    return;
  }

  MessagePort* port = MessagePort::Create(*this);
  port->Entangle(std::move(channel));
  MessageEvent* event = MessageEvent::Create(new MessagePortArray(1, port),
                                             String(), String(), port);
  event->initEvent(EventTypeNames::connect, false, false);
  DispatchEvent(*event);
}

void SharedWorkerGlobalScope::ExceptionThrown(ErrorEvent* event) {
  WorkerGlobalScope::ExceptionThrown(event);
  if (WorkerThreadDebugger* debugger =
          WorkerThreadDebugger::From(GetThread()->GetIsolate()))
    debugger->ExceptionThrown(GetThread(), event);
}

void SharedWorkerGlobalScope::Trace(blink::Visitor* visitor) {
  WorkerGlobalScope::Trace(visitor);
}

}  // namespace blink
