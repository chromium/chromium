// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mojo/test/mojo_interface_interceptor.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
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
    const String& scope,
    bool use_browser_interface_broker,
    ExceptionState& exception_state) {
  bool process_scope = scope == "process";
  if (process_scope && !context->IsDocument()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "\"process\" scope interception is unavailable outside a Document.");
    return nullptr;
  }

  return MakeGarbageCollected<MojoInterfaceInterceptor>(
      context, interface_name, process_scope,
      UseBrowserInterfaceBroker(use_browser_interface_broker));
}

MojoInterfaceInterceptor::~MojoInterfaceInterceptor() = default;

void MojoInterfaceInterceptor::start(ExceptionState& exception_state) {
  if (started_)
    return;

  service_manager::InterfaceProvider* interface_provider =
      GetInterfaceProvider();
  if (!interface_provider) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The interface provider is unavailable.");
    return;
  }

  std::string interface_name = interface_name_.Utf8();

  if (process_scope_) {
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

  if (use_browser_interface_broker_) {
    ExecutionContext* context = GetExecutionContext();

    if (!context)
      return;

    started_ = true;
    if (!context->GetBrowserInterfaceBroker().SetBinderForTesting(
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

  // TODO(crbug.com/994843): remove when no longer used.
  service_manager::InterfaceProvider::TestApi test_api(interface_provider);
  if (test_api.HasBinderForName(interface_name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidModificationError,
        "Interface " + interface_name_ +
            " is already intercepted by another MojoInterfaceInterceptor.");
    return;
  }

  started_ = true;
  test_api.SetBinderForName(
      interface_name,
      WTF::BindRepeating(&MojoInterfaceInterceptor::OnInterfaceRequest,
                         WrapWeakPersistent(this)));
}

void MojoInterfaceInterceptor::stop() {
  if (!started_)
    return;

  started_ = false;
  std::string interface_name = interface_name_.Utf8();

  if (process_scope_) {
    Platform::Current()->GetBrowserInterfaceBroker()->SetBinderForTesting(
        interface_name, {});
    return;
  }

  if (use_browser_interface_broker_) {
    ExecutionContext* context = GetExecutionContext();
    DCHECK(context);
    context->GetBrowserInterfaceBroker().SetBinderForTesting(interface_name,
                                                             {});
    return;
  }

  // TODO(crbug.com/994843): remove when no longer used.
  // GetInterfaceProvider() is guaranteed not to return nullptr because this
  // method is called when the context is destroyed.
  service_manager::InterfaceProvider::TestApi test_api(GetInterfaceProvider());
  DCHECK(test_api.HasBinderForName(interface_name));
  test_api.ClearBinderForName(interface_name);
}

void MojoInterfaceInterceptor::Trace(blink::Visitor* visitor) {
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

const AtomicString& MojoInterfaceInterceptor::InterfaceName() const {
  return event_target_names::kMojoInterfaceInterceptor;
}

ExecutionContext* MojoInterfaceInterceptor::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

bool MojoInterfaceInterceptor::HasPendingActivity() const {
  return started_;
}

void MojoInterfaceInterceptor::ContextDestroyed(ExecutionContext*) {
  stop();
}

MojoInterfaceInterceptor::MojoInterfaceInterceptor(
    ExecutionContext* context,
    const String& interface_name,
    bool process_scope,
    UseBrowserInterfaceBroker use_browser_interface_broker)
    : ContextLifecycleObserver(context),
      interface_name_(interface_name),
      process_scope_(process_scope),
      use_browser_interface_broker_(use_browser_interface_broker) {}

service_manager::InterfaceProvider*
MojoInterfaceInterceptor::GetInterfaceProvider() const {
  ExecutionContext* context = GetExecutionContext();
  if (!context)
    return nullptr;

  return context->GetInterfaceProvider();
}

void MojoInterfaceInterceptor::OnInterfaceRequest(
    mojo::ScopedMessagePipeHandle handle) {
  // Execution of JavaScript may be forbidden in this context as this method is
  // called synchronously by the InterfaceProvider. Dispatching of the
  // 'interfacerequest' event is therefore scheduled to take place in the next
  // microtask. This also more closely mirrors the behavior when an interface
  // request is being satisfied by another process.
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMicrotask)
      ->PostTask(
          FROM_HERE,
          WTF::Bind(&MojoInterfaceInterceptor::DispatchInterfaceRequestEvent,
                    WrapPersistent(this), WTF::Passed(std::move(handle))));
}

void MojoInterfaceInterceptor::DispatchInterfaceRequestEvent(
    mojo::ScopedMessagePipeHandle handle) {
  DispatchEvent(*MakeGarbageCollected<MojoInterfaceRequestEvent>(
      MakeGarbageCollected<MojoHandle>(
          mojo::ScopedHandle::From(std::move(handle)))));
}

}  // namespace blink
