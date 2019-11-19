// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_H_

#include <memory>
#include "base/macros.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/fetch/global_fetch.h"
#include "third_party/blink/renderer/modules/cache_storage/cache.h"
#include "third_party/blink/renderer/modules/cache_storage/multi_cache_query_options.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class CacheStorageBlobClientList;

class CacheStorage final : public ScriptWrappable,
                           public ActiveScriptWrappable<CacheStorage>,
                           public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(CacheStorage);

 public:
  CacheStorage(ExecutionContext*, GlobalFetch::ScopedFetcher*);
  ~CacheStorage() override;

  ScriptPromise open(ScriptState*, const String& cache_name);
  ScriptPromise has(ScriptState*, const String& cache_name);
  ScriptPromise Delete(ScriptState*, const String& cache_name);
  ScriptPromise keys(ScriptState*);
  ScriptPromise match(ScriptState*,
                      const RequestInfo&,
                      const MultiCacheQueryOptions*,
                      ExceptionState&);

  bool HasPendingActivity() const override;
  void Trace(blink::Visitor*) override;
  void ContextDestroyed(ExecutionContext*) override;

 private:
  ScriptPromise MatchImpl(ScriptState*,
                          const Request*,
                          const MultiCacheQueryOptions*);

  bool IsAllowed(ScriptState*);

  Member<GlobalFetch::ScopedFetcher> scoped_fetcher_;
  Member<CacheStorageBlobClientList> blob_client_list_;

  mojo::Remote<mojom::blink::CacheStorage> cache_storage_remote_;
  base::Optional<bool> allowed_;
  bool ever_used_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorage);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_H_
