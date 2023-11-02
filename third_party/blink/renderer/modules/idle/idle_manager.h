// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_MANAGER_H_

#include "third_party/blink/public/mojom/idle/idle_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT IdleManager final : public GarbageCollected<IdleManager>,
                                         public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];

  static IdleManager* From(ExecutionContext*);

  explicit IdleManager(ExecutionContext*);
  ~IdleManager();

  ScriptPromise RequestPermission(ScriptState*, ExceptionState&);
  void AddMonitor(mojo::PendingRemote<mojom::blink::IdleMonitor>,
                  mojom::blink::IdleManager::AddMonitorCallback);

  void Trace(Visitor*) const override;

  void InitForTesting(
      mojo::PendingRemote<mojom::blink::IdleManager> idle_service);

 private:
  void OnPermissionRequestComplete(ScriptPromiseResolver*,
                                   mojom::blink::PermissionStatus);

  HeapMojoRemote<mojom::blink::IdleManager> idle_service_;
  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_MANAGER_H_
