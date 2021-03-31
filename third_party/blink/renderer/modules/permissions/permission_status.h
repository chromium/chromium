// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_PERMISSION_STATUS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_PERMISSION_STATUS_H_

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class ScriptPromiseResolver;
class Permissions;

// Expose the status of a given permission type for the current
// ExecutionContext.
class PermissionStatus final : public EventTargetWithInlineData,
                               public ActiveScriptWrappable<PermissionStatus>,
                               public ExecutionContextLifecycleStateObserver,
                               public mojom::blink::PermissionObserver {
  DEFINE_WRAPPERTYPEINFO();

  using MojoPermissionDescriptor = mojom::blink::PermissionDescriptorPtr;
  using MojoPermissionStatus = mojom::blink::PermissionStatus;

 public:
  static PermissionStatus* Take(Permissions&,
                                ScriptPromiseResolver*,
                                MojoPermissionStatus,
                                MojoPermissionDescriptor);

  static PermissionStatus* CreateAndListen(Permissions&,
                                           ExecutionContext*,
                                           MojoPermissionStatus,
                                           MojoPermissionDescriptor);

  PermissionStatus(Permissions&,
                   ExecutionContext*,
                   MojoPermissionStatus,
                   MojoPermissionDescriptor);
  ~PermissionStatus() override;

  // EventTarget implementation.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

  // ExecutionContextLifecycleStateObserver implementation.
  void ContextLifecycleStateChanged(mojom::FrameLifecycleState) override;
  void ContextDestroyed() override {}

  String state() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(change, kChange)

  void Trace(Visitor*) const override;

 private:
  void StartListening();
  void StopListening();

  void OnPermissionStatusChange(MojoPermissionStatus) override;

  MojoPermissionStatus status_;
  MojoPermissionDescriptor descriptor_;
  HeapMojoReceiver<mojom::blink::PermissionObserver, PermissionStatus>
      receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_PERMISSION_STATUS_H_
