// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_INSPECTOR_CACHE_STORAGE_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_INSPECTOR_CACHE_STORAGE_AGENT_H_

#include <memory>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/CacheStorage.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class InspectedFrames;

class MODULES_EXPORT InspectorCacheStorageAgent final
    : public InspectorBaseAgent<protocol::CacheStorage::Metainfo> {
 public:
  using CachesMap = HashMap<String, mojo::Remote<mojom::blink::CacheStorage>>;

  explicit InspectorCacheStorageAgent(InspectedFrames*);
  ~InspectorCacheStorageAgent() override;
  void Trace(blink::Visitor*) override;

  void requestCacheNames(const String& security_origin,
                         std::unique_ptr<RequestCacheNamesCallback>) override;
  void requestEntries(const String& cache_id,
                      protocol::Maybe<int> skip_count,
                      protocol::Maybe<int> page_size,
                      protocol::Maybe<String> path_filter,
                      std::unique_ptr<RequestEntriesCallback>) override;
  void deleteCache(const String& cache_id,
                   std::unique_ptr<DeleteCacheCallback>) override;
  void deleteEntry(const String& cache_id,
                   const String& request,
                   std::unique_ptr<DeleteEntryCallback>) override;
  void requestCachedResponse(
      const String& cache_id,
      const String& request_url,
      const std::unique_ptr<protocol::Array<protocol::CacheStorage::Header>>
          request_headers,
      std::unique_ptr<RequestCachedResponseCallback>) override;

 private:
  Member<InspectedFrames> frames_;

  CachesMap caches_;

  DISALLOW_COPY_AND_ASSIGN(InspectorCacheStorageAgent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_INSPECTOR_CACHE_STORAGE_AGENT_H_
