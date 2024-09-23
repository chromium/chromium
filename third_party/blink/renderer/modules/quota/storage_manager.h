// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_QUOTA_STORAGE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_QUOTA_STORAGE_MANAGER_H_

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/quota/quota_manager_host.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace blink {

class ExecutionContext;
class ScriptState;
class StorageEstimate;

class StorageManager final : public EventTarget,
                             public ExecutionContextClient,
                             public mojom::blink::QuotaChangeListener {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit StorageManager(ExecutionContext*);
  ~StorageManager() override;

  ScriptPromise<IDLBoolean> persisted(ScriptState*, ExceptionState&);
  ScriptPromise<IDLBoolean> persist(ScriptState*, ExceptionState&);

  ScriptPromise<StorageEstimate> estimate(ScriptState*, ExceptionState&);

  void Trace(Visitor* visitor) const override;

  // EventTarget
  DEFINE_ATTRIBUTE_EVENT_LISTENER(quotachange, kQuotachange)
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // network::mojom::blink::QuotaChangeListener
  void OnQuotaChange() override;

 protected:
  // EventTarget overrides.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) final;
  void RemovedEventListener(const AtomicString& event_type,
                            const RegisteredEventListener&) final;

 private:
  mojom::blink::PermissionService* GetPermissionService(ExecutionContext*);

  void PermissionServiceConnectionError();
  void PermissionRequestComplete(ScriptPromiseResolver<IDLBoolean>*,
                                 mojom::blink::PermissionStatus);

  // Called when a quota change event listener is added.
  void StartObserving();

  // Called when all the change event listeners have been removed.
  void StopObserving();

  // Binds the interface (if not already bound) with the given interface
  // provider, and returns it,
  mojom::blink::QuotaManagerHost* GetQuotaHost(ExecutionContext*);

  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;
  HeapMojoRemote<mojom::blink::QuotaManagerHost> quota_host_;

  HeapMojoReceiver<mojom::blink::QuotaChangeListener, StorageManager>
      change_listener_receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_QUOTA_STORAGE_MANAGER_H_
