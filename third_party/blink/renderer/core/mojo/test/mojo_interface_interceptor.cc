// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mojo/test/mojo_interface_interceptor.h"

#include <utility>

#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/mojo/mojo_handle.h"
#include "third_party/blink/renderer/core/mojo/test/mojo_interface_request_event.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

// static
MojoInterfaceInterceptor* MojoInterfaceInterceptor::Create(
    ExecutionContext* context,
    const String& interface_name,
    const Scope& scope,
    ExceptionState& exception_state) {
  if (scope == Scope::Enum::kProcess && !context->IsWindow()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "\"process\" scope interception is unavailable outside a Document.");
    return nullptr;
  }

  if (scope == Scope::Enum::kContextJs &&
      !context->use_mojo_js_interface_broker()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "\"context_js\" scope interception is unavailable unless MojoJS "
        "interface broker is used.");
    return nullptr;
  }

  return MakeGarbageCollected<MojoInterfaceInterceptor>(context, interface_name,
                                                        scope.AsEnum());
}

MojoInterfaceInterceptor::~MojoInterfaceInterceptor() = default;

void MojoInterfaceInterceptor::start(ExceptionState& exception_state) {
  if (started_)
    return;

  std::string interface_name = interface_name_.Utf8();

  if (scope_ == Scope::Enum::kProcess) {
    started_ = true;
    if (!Platform::Current()->GetBrowserInterfaceBroker()->SetBinderForTesting(
            interface_name,
            WTF::BindRepeating(&MojoInterfaceInterceptor::OnInterfaceRequest,
                               WrapWeakPersistent(this)))) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidModificationError,
          "Interface " + interface_name_ +
              " is already intercepted by another MojoInterfaceInterceptor.");
    }

    return;
  }

  ExecutionContext* context = GetExecutionContext();

  if (!context)
    return;

  started_ = true;
  if (scope_ == Scope::Enum::kContextJs) {
    DCHECK(context->use_mojo_js_interface_broker());
    if (!context->GetMojoJSInterfaceBroker().SetBinderForTesting(
            interface_name,
            WTF::BindRepeating(&MojoInterfaceInterceptor::OnInterfaceRequest,
                               WrapWeakPersistent(this)))) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidModificationError,
          "Interface " + interface_name_ +
              " is already intercepted by another MojoInterfaceInterceptor.");
    }
    return;
  }

  if (!context->GetBrowserInterfaceBroker().SetBinderForTesting(
          interface_name,
          WTF::BindRepeating(&MojoInterfaceInterceptor::OnInterfaceRequest,
                             WrapWeakPersistent(this)))) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Interface " + interface_name_ +
            " is already intercepted by another MojoInterfaceInterceptor.");
  }
}

void MojoInterfaceInterceptor::stop() {
  if (!started_)
    return;

  started_ = false;
  std::string interface_name = interface_name_.Utf8();

  if (scope_ == Scope::Enum::kProcess) {
    Platform::Current()->GetBrowserInterfaceBroker()->SetBinderForTesting(
        interface_name, {});
    return;
  }

  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);

  if (scope_ == Scope::Enum::kContextJs) {
    DCHECK(context->use_mojo_js_interface_broker());
    context->GetMojoJSInterfaceBroker().SetBinderForTesting(interface_name, {});
    return;
  }

  context->GetBrowserInterfaceBroker().SetBinderForTesting(interface_name, {});
}

void MojoInterfaceInterceptor::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

const AtomicString& MojoInterfaceInterceptor::InterfaceName() const {
  return event_target_names::kMojoInterfaceInterceptor;
}

ExecutionContext* MojoInterfaceInterceptor::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

bool MojoInterfaceInterceptor::HasPendingActivity() const {
  return started_;
}

void MojoInterfaceInterceptor::ContextDestroyed() {
  stop();
}

MojoInterfaceInterceptor::MojoInterfaceInterceptor(ExecutionContext* context,
                                                   const String& interface_name,
                                                   Scope::Enum scope)
    : ActiveScriptWrappable<MojoInterfaceInterceptor>({}),
      ExecutionContextLifecycleObserver(context),
      interface_name_(interface_name),
      scope_(scope) {}

void MojoInterfaceInterceptor::OnInterfaceRequest(
    mojo::ScopedMessagePipeHandle handle) {
  // Execution of JavaScript may be forbidden in this context as this method is
  // called synchronously by the BrowserInterfaceBroker. Dispatching of the
  // 'interfacerequest' event is therefore scheduled to take place in the next
  // microtask. This also more closely mirrors the behavior when an interface
  // request is being satisfied by another process.
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMicrotask)
      ->PostTask(FROM_HERE,
                 WTF::BindOnce(
                     &MojoInterfaceInterceptor::DispatchInterfaceRequestEvent,
                     WrapPersistent(this), std::move(handle)));
}

void MojoInterfaceInterceptor::DispatchInterfaceRequestEvent(
    mojo::ScopedMessagePipeHandle handle) {
  DispatchEvent(*MakeGarbageCollected<MojoInterfaceRequestEvent>(
      MakeGarbageCollected<MojoHandle>(
          mojo::ScopedHandle::From(std::move(handle)))));
}

}  // namespace blink
