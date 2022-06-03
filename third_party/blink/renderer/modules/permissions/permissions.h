// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_PERMISSIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_PERMISSIONS_H_

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;
class NavigatorBase;
class ScriptPromiseResolver;
class ScriptState;
class ScriptValue;

class Permissions final : public ScriptWrappable,
                          public Supplement<NavigatorBase>,
                          public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // Getter for navigator.permissions
  static Permissions* permissions(NavigatorBase&);

  explicit Permissions(NavigatorBase&);

  ScriptPromise query(ScriptState*, const ScriptValue&, ExceptionState&);
  ScriptPromise request(ScriptState*, const ScriptValue&, ExceptionState&);
  ScriptPromise revoke(ScriptState*, const ScriptValue&, ExceptionState&);
  ScriptPromise requestAll(ScriptState*,
                           const HeapVector<ScriptValue>&,
                           ExceptionState&);

  // ExecutionContextLifecycleStateObserver:
  void ContextDestroyed() override;

  void PermissionStatusObjectCreated() { ++created_permission_status_objects_; }

  void Trace(Visitor*) const override;

 private:
  mojom::blink::PermissionService* GetService(ExecutionContext*);
  void ServiceConnectionError();
  void TaskComplete(ScriptPromiseResolver*,
                    mojom::blink::PermissionDescriptorPtr,
                    mojom::blink::PermissionStatus);
  void BatchTaskComplete(ScriptPromiseResolver*,
                         Vector<mojom::blink::PermissionDescriptorPtr>,
                         Vector<int>,
                         const Vector<mojom::blink::PermissionStatus>&);

  int created_permission_status_objects_ = 0;

  HeapMojoRemote<mojom::blink::PermissionService> service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_PERMISSIONS_H_
