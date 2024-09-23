// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/idle/idle_manager.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_permission_state.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
const char IdleManager::kSupplementName[] = "IdleManager";

// static
IdleManager* IdleManager::From(ExecutionContext* context) {
  DCHECK(context);
  DCHECK(context->IsContextThread());

  IdleManager* manager =
      Supplement<ExecutionContext>::From<IdleManager>(context);
  if (!manager) {
    manager = MakeGarbageCollected<IdleManager>(context);
    Supplement<ExecutionContext>::ProvideTo(*context, manager);
  }

  return manager;
}

IdleManager::IdleManager(ExecutionContext* context)
    : Supplement<ExecutionContext>(*context),
      idle_service_(context),
      permission_service_(context) {}

IdleManager::~IdleManager() = default;

ScriptPromise<V8PermissionState> IdleManager::RequestPermission(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  ExecutionContext* context = GetSupplementable();
  DCHECK_EQ(context, ExecutionContext::From(script_state));

  // This function is annotated with [Exposed=Window].
  DCHECK(context->IsWindow());
  auto* window = To<LocalDOMWindow>(context);

  if (!LocalFrame::HasTransientUserActivation(window->GetFrame())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Must be handling a user gesture to show a permission request.");
    return EmptyPromise();
  }

  // This interface is annotated with [SecureContext].
  DCHECK(context->IsSecureContext());

  if (!permission_service_.is_bound()) {
    // See https://bit.ly/2S0zRAS for task types.
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        context->GetTaskRunner(TaskType::kMiscPlatformAPI);
    ConnectToPermissionService(
        context,
        permission_service_.BindNewPipeAndPassReceiver(std::move(task_runner)));
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<V8PermissionState>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  permission_service_->RequestPermission(
      CreatePermissionDescriptor(mojom::blink::PermissionName::IDLE_DETECTION),
      LocalFrame::HasTransientUserActivation(window->GetFrame()),
      WTF::BindOnce(&IdleManager::OnPermissionRequestComplete,
                    WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

void IdleManager::AddMonitor(
    mojo::PendingRemote<mojom::blink::IdleMonitor> monitor,
    mojom::blink::IdleManager::AddMonitorCallback callback) {
  if (!idle_service_.is_bound()) {
    ExecutionContext* context = GetSupplementable();
    // See https://bit.ly/2S0zRAS for task types.
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        context->GetTaskRunner(TaskType::kMiscPlatformAPI);
    context->GetBrowserInterfaceBroker().GetInterface(
        idle_service_.BindNewPipeAndPassReceiver(std::move(task_runner)));
  }

  idle_service_->AddMonitor(std::move(monitor), std::move(callback));
}

void IdleManager::Trace(Visitor* visitor) const {
  visitor->Trace(idle_service_);
  visitor->Trace(permission_service_);
  Supplement<ExecutionContext>::Trace(visitor);
}

void IdleManager::InitForTesting(
    mojo::PendingRemote<mojom::blink::IdleManager> idle_service) {
  ExecutionContext* context = GetSupplementable();
  // See https://bit.ly/2S0zRAS for task types.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      context->GetTaskRunner(TaskType::kMiscPlatformAPI);
  idle_service_.Bind(std::move(idle_service), std::move(task_runner));
}

void IdleManager::OnPermissionRequestComplete(
    ScriptPromiseResolver<V8PermissionState>* resolver,
    mojom::blink::PermissionStatus status) {
  resolver->Resolve(PermissionStatusToString(status));
}

}  // namespace blink
