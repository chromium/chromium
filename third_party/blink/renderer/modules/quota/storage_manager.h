// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_QUOTA_STORAGE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_QUOTA_STORAGE_MANAGER_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/quota/quota_dispatcher_host.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class ExecutionContext;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class StorageManager final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ScriptPromise persisted(ScriptState*);
  ScriptPromise persist(ScriptState*);

  ScriptPromise estimate(ScriptState*);

 private:
  mojom::blink::PermissionService* GetPermissionService(ExecutionContext*);
  void PermissionServiceConnectionError();
  void PermissionRequestComplete(ScriptPromiseResolver*,
                                 mojom::blink::PermissionStatus);

  // Binds the interface (if not already bound) with the given interface
  // provider, and returns it,
  mojom::blink::QuotaDispatcherHost* GetQuotaHost(ExecutionContext*);

  mojo::Remote<mojom::blink::PermissionService> permission_service_;
  mojo::Remote<mojom::blink::QuotaDispatcherHost> quota_host_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_QUOTA_STORAGE_MANAGER_H_
