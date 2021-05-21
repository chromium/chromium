// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_H_

#include "base/macros.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/fetch/global_fetch.h"
#include "third_party/blink/renderer/modules/cache_storage/cache.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class CacheStorageBlobClientList;
class MultiCacheQueryOptions;

class CacheStorage final : public ScriptWrappable,
                           public ActiveScriptWrappable<CacheStorage>,
                           public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CacheStorage(ExecutionContext*, GlobalFetch::ScopedFetcher*);
  ~CacheStorage() override;

  ScriptPromise open(ScriptState*, const String& cache_name);
  ScriptPromise has(ScriptState*, const String& cache_name);
  ScriptPromise Delete(ScriptState*, const String& cache_name);
  ScriptPromise keys(ScriptState*);
  ScriptPromise match(ScriptState* script_state,
                      const V8RequestInfo* request,
                      const MultiCacheQueryOptions* options,
                      ExceptionState& exception_state);

  bool HasPendingActivity() const override;
  void Trace(Visitor*) const override;

 private:
  ScriptPromise MatchImpl(ScriptState*,
                          const Request*,
                          const MultiCacheQueryOptions*);

  bool IsAllowed(ScriptState*);

  void MaybeInit();

  Member<GlobalFetch::ScopedFetcher> scoped_fetcher_;
  Member<CacheStorageBlobClientList> blob_client_list_;

  HeapMojoRemote<mojom::blink::CacheStorage> cache_storage_remote_;
  absl::optional<bool> allowed_;
  bool ever_used_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorage);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_H_
