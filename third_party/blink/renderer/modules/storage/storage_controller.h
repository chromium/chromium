// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_STORAGE_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_STORAGE_CONTROLLER_H_

#include <memory>

#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/mojom/dom_storage/storage_partition_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/storage/storage_area.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class CachedStorageArea;
class InspectorDOMStorageAgent;
class LocalFrame;
class SecurityOrigin;
class StorageNamespace;

// Singleton that manages the creation & accounting for DOMStorage objects. It
// does this by holding weak references to all session storage namespaces, and
// owning the local storage namespace internally. The total cache size is
// exposed with |TotalCacheSize()|, and |ClearAreasIfNeeded()| will - if our
// total cache size is larger than |total_cache_limit| - clear away any cache
// areas in live namespaces that no longer have references from Blink objects.
//
// SessionStorage StorageNamespace objects are created with
// |CreateSessionStorageNamespace| and live as a supplement on the Page.
//
// The LocalStorage StorageNamespace object is owned internally, and
// StorageController delegates the following methods to that namespace:
// GetLocalStorageArea, GetWebLocalStorageArea,
// AddLocalStorageInspectorStorageAgent,
// RemoveLocalStorageInspectorStorageAgent, DidDispatchLocalStorageEvent
class MODULES_EXPORT StorageController {
  USING_FAST_MALLOC(StorageController);

 public:
  // Returns the one global StorageController instance.
  static StorageController* GetInstance();

  static bool CanAccessStorageArea(LocalFrame* frame,
                                   StorageArea::StorageType type);

  // Visible for testing.
  StorageController(scoped_refptr<base::SingleThreadTaskRunner> ipc_runner,
                    mojo::PendingRemote<mojom::blink::StoragePartitionService>
                        storage_partition_service,
                    size_t total_cache_limit);

  // Creates a MakeGarbageCollected<StorageNamespace> for Session storage, and
  // holds a weak reference for accounting & clearing. If there is already a
  // StorageNamespace created for the given id, it is returned.
  StorageNamespace* CreateSessionStorageNamespace(const String& namespace_id);

  // Returns the total size of all cached areas in namespaces this controller
  // knows of.
  size_t TotalCacheSize() const;

  // Cleans up unused areas if the total cache size is over the cache limit.
  void ClearAreasIfNeeded();

  // Methods that delegate to the internal SessionNamespace used for
  // LocalStorage:

  scoped_refptr<CachedStorageArea> GetLocalStorageArea(const SecurityOrigin*);
  void AddLocalStorageInspectorStorageAgent(InspectorDOMStorageAgent* agent);
  void RemoveLocalStorageInspectorStorageAgent(InspectorDOMStorageAgent* agent);
  void DidDispatchLocalStorageEvent(const SecurityOrigin* origin,
                                    const String& key,
                                    const String& old_value,
                                    const String& new_value);

  mojom::blink::StoragePartitionService* storage_partition_service() const {
    return storage_partition_service_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> IPCTaskRunner() {
    return ipc_runner_;
  }

 private:
  void EnsureLocalStorageNamespaceCreated();

  scoped_refptr<base::SingleThreadTaskRunner> ipc_runner_;
  Persistent<HeapHashMap<String, WeakMember<StorageNamespace>>> namespaces_;
  Persistent<StorageNamespace> local_storage_namespace_;
  size_t total_cache_limit_;

  mojo::Remote<mojom::blink::StoragePartitionService>
      storage_partition_service_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_STORAGE_CONTROLLER_H_
