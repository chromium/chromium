/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_STORAGE_NAMESPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_STORAGE_NAMESPACE_H_

#include <memory>

#include "third_party/blink/public/mojom/dom_storage/session_storage_namespace.mojom-blink.h"
#include "third_party/blink/public/mojom/dom_storage/storage_partition_service.mojom-blink.h"
#include "third_party/blink/public/platform/web_storage_area.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/storage/cached_storage_area.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class InspectorDOMStorageAgent;
class StorageController;
class SecurityOrigin;
class WebStorageNamespace;
class WebViewClient;

// Contains DOMStorage storage areas for origins & handles inspector agents. A
// namespace is either a SessionStorage namespace with a namespace_id, or a
// LocalStorage namespace with no (or an empty) namespace_id. The LocalStorage
// version of the StorageNamespace lives in the StorageController.
// InspectorDOMStorageAgents that are registered on this object are notified
// through |DidDispatchStorageEvent|.
//
// With the kOnionSoupDOMStorage flag off:
// The StorageNamespace basically delegates calls to GetWebStorageArea to the
// internal WebStorageNamespace. |GetWebStorageArea| is used to get the storage
// area for an origin.
//
// With the kOnionSoupDOMStorage flag on:
// The StorageNamespace for SessioStorage supplement the Page. |GetCachedArea|
// is used to get the storage area for an origin.
class MODULES_EXPORT StorageNamespace final
    : public GarbageCollectedFinalized<StorageNamespace>,
      public Supplement<Page>,
      public CachedStorageArea::InspectorEventListener {
  USING_GARBAGE_COLLECTED_MIXIN(StorageNamespace);

 public:
  static const char kSupplementName[];

  static void ProvideSessionStorageNamespaceTo(Page&, WebViewClient*);
  static StorageNamespace* From(Page* page) {
    return Supplement<Page>::From<StorageNamespace>(page);
  }

  // Constructor for an onion-souped LocalStorage namespace.
  StorageNamespace(StorageController*);
  // Constructor for an onion-souped SessionStorage namespace.
  StorageNamespace(StorageController*, const String& namespace_id);
  // Pre-onion-soup constructor. WebStorageNamespace must not be null.
  StorageNamespace(std::unique_ptr<WebStorageNamespace>);

  ~StorageNamespace() override;

  // TODO(dmurph): Remove this once Onion Soupified.
  const String& namespace_id() { return namespace_id_; }
  // TODO(dmurph): Remove this once Onion Soupified.
  std::unique_ptr<WebStorageArea> GetWebStorageArea(const SecurityOrigin*);

  scoped_refptr<CachedStorageArea> GetCachedArea(const SecurityOrigin* origin);

  // Only valid to call this if |this| and |target| are session storage
  // namespaces.
  void CloneTo(const String& target);

  size_t TotalCacheSize() const;

  // Removes any CachedStorageAreas that aren't referenced by any source.
  void CleanUpUnusedAreas();

  bool IsSessionStorage() const { return !namespace_id_.IsEmpty(); }

  void AddInspectorStorageAgent(InspectorDOMStorageAgent* agent);
  void RemoveInspectorStorageAgent(InspectorDOMStorageAgent* agent);

  void Trace(Visitor* visitor) override;

  // Iterates all of the inspector agents and calls
  // |DidDispatchDOMStorageEvent|.
  void DidDispatchStorageEvent(const SecurityOrigin* origin,
                               const String& key,
                               const String& old_value,
                               const String& new_value) override;

 private:
  void EnsureConnected();

  HeapHashSet<WeakMember<InspectorDOMStorageAgent>> inspector_agents_;

  // Onion-souped storage wiring, not turned on yet.
  // Lives globally.
  StorageController* controller_;
  String namespace_id_;
  mojom::blink::SessionStorageNamespacePtr namespace_;
  HashMap<scoped_refptr<const SecurityOrigin>,
          scoped_refptr<CachedStorageArea>,
          SecurityOriginHash>
      cached_areas_;

  // Pre-onion-soup storage wiring, currently active.
  std::unique_ptr<WebStorageNamespace> web_storage_namespace_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_STORAGE_NAMESPACE_H_
