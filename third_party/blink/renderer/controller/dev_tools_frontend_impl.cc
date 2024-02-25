/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/controller/dev_tools_frontend_impl.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dev_tools_host.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_host.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/script/classic_script.h"

namespace blink {

// static
void DevToolsFrontendImpl::BindMojoRequest(
    LocalFrame* local_frame,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsFrontend> receiver) {
  if (!local_frame)
    return;
  local_frame->ProvideSupplement(MakeGarbageCollected<DevToolsFrontendImpl>(
      *local_frame, std::move(receiver)));
}

// static
DevToolsFrontendImpl* DevToolsFrontendImpl::From(LocalFrame* local_frame) {
  if (!local_frame)
    return nullptr;
  return local_frame->RequireSupplement<DevToolsFrontendImpl>();
}

// static
const char DevToolsFrontendImpl::kSupplementName[] = "DevToolsFrontendImpl";

DevToolsFrontendImpl::DevToolsFrontendImpl(
    LocalFrame& frame,
    mojo::PendingAssociatedReceiver<mojom::blink::DevToolsFrontend> receiver)
    : Supplement<LocalFrame>(frame) {
  receiver_.Bind(std::move(receiver),
                 frame.GetTaskRunner(TaskType::kMiscPlatformAPI));
}

DevToolsFrontendImpl::~DevToolsFrontendImpl() = default;

void DevToolsFrontendImpl::DidClearWindowObject() {
  if (host_.is_bound()) {
    v8::Isolate* isolate = GetSupplementable()->DomWindow()->GetIsolate();
    // Use higher limit for DevTools isolate so that it does not OOM when
    // profiling large heaps.
    isolate->IncreaseHeapLimitForDebugging();
    ScriptState* script_state = ToScriptStateForMainWorld(GetSupplementable());
    DCHECK(script_state);
    ScriptState::Scope scope(script_state);
    v8::MicrotasksScope microtasks_scope(
        isolate, ToMicrotaskQueue(script_state),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    if (devtools_host_)
      devtools_host_->DisconnectClient();
    devtools_host_ =
        MakeGarbageCollected<DevToolsHost>(this, GetSupplementable());
    v8::Local<v8::Value> devtools_host_obj =
        ToV8Traits<DevToolsHost>::ToV8(script_state, devtools_host_.Get());
    DCHECK(!devtools_host_obj.IsEmpty());
    script_state->GetContext()
        ->Global()
        ->Set(script_state->GetContext(),
              V8AtomicString(isolate, "DevToolsHost"), devtools_host_obj)
        .Check();
  }

  if (!api_script_.empty()) {
    ClassicScript::CreateUnspecifiedScript(api_script_)
        ->RunScript(GetSupplementable()->DomWindow());
  }
}

void DevToolsFrontendImpl::SetupDevToolsFrontend(
    const String& api_script,
    mojo::PendingAssociatedRemote<mojom::blink::DevToolsFrontendHost> host) {
  LocalFrame* frame = GetSupplementable();
  DCHECK(frame->IsMainFrame());
  if (frame->GetWidgetForLocalRoot()) {
    frame->GetWidgetForLocalRoot()->SetLayerTreeDebugState(
        cc::LayerTreeDebugState());
  } else {
    frame->AddWidgetCreationObserver(this);
  }
  frame->GetPage()->GetSettings().SetForceDarkModeEnabled(false);
  api_script_ = api_script;
  host_.Bind(std::move(host),
             GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI));
  host_.set_disconnect_handler(WTF::BindOnce(
      &DevToolsFrontendImpl::DestroyOnHostGone, WrapWeakPersistent(this)));
  GetSupplementable()->GetPage()->SetDefaultPageScaleLimits(1.f, 1.f);
}

void DevToolsFrontendImpl::OnLocalRootWidgetCreated() {
  GetSupplementable()->GetWidgetForLocalRoot()->SetLayerTreeDebugState(
      cc::LayerTreeDebugState());
}

void DevToolsFrontendImpl::SetupDevToolsExtensionAPI(
    const String& extension_api) {
  DCHECK(!GetSupplementable()->IsMainFrame());
  api_script_ = extension_api;
}

void DevToolsFrontendImpl::SendMessageToEmbedder(base::Value::Dict message) {
  if (host_.is_bound())
    host_->DispatchEmbedderMessage(std::move(message));
}

void DevToolsFrontendImpl::DestroyOnHostGone() {
  if (devtools_host_)
    devtools_host_->DisconnectClient();
  GetSupplementable()->RemoveSupplement<DevToolsFrontendImpl>();
}

void DevToolsFrontendImpl::Trace(Visitor* visitor) const {
  visitor->Trace(devtools_host_);
  visitor->Trace(host_);
  visitor->Trace(receiver_);
  Supplement<LocalFrame>::Trace(visitor);
}

}  // namespace blink
