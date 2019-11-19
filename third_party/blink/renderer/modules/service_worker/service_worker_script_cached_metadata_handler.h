// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CACHED_METADATA_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CACHED_METADATA_HANDLER_H_

#include <stdint.h>
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CachedMetadata;
class ServiceWorkerGlobalScope;

class ServiceWorkerScriptCachedMetadataHandler
    : public SingleCachedMetadataHandler {
 public:
  static ServiceWorkerScriptCachedMetadataHandler* Create(
      ServiceWorkerGlobalScope* global_scope,
      const KURL& script_url,
      std::unique_ptr<Vector<uint8_t>> meta_data) {
    return MakeGarbageCollected<ServiceWorkerScriptCachedMetadataHandler>(
        global_scope, script_url, std::move(meta_data));
  }

  ServiceWorkerScriptCachedMetadataHandler(
      ServiceWorkerGlobalScope*,
      const KURL& script_url,
      std::unique_ptr<Vector<uint8_t>> meta_data);
  ~ServiceWorkerScriptCachedMetadataHandler() override;
  void Trace(blink::Visitor*) override;
  void SetCachedMetadata(uint32_t data_type_id,
                         const uint8_t*,
                         size_t,
                         CacheType) override;
  void ClearCachedMetadata(CacheType) override;
  scoped_refptr<CachedMetadata> GetCachedMetadata(
      uint32_t data_type_id) const override;
  String Encoding() const override;
  bool IsServedFromCacheStorage() const override;
  void OnMemoryDump(WebProcessMemoryDump* pmd,
                    const String& dump_prefix) const override;
  size_t GetCodeCacheSize() const override;

 private:
  Member<ServiceWorkerGlobalScope> global_scope_;
  KURL script_url_;
  scoped_refptr<CachedMetadata> cached_metadata_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CACHED_METADATA_HANDLER_H_
