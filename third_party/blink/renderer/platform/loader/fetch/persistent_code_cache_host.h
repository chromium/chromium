// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_PERSISTENT_CODE_CACHE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_PERSISTENT_CODE_CACHE_HOST_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/wtf/sequence_bound.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Implementation that uses a PersistentCache for local fetches on a background
// sequence. Two connections to `PersistentCodeCacheHost` instances in the
// browser process are maintained; one for compiled JavaScript, and one for
// compiled WASM. All operations take places on a blocking sequence; calls to
// the browser process (to get a connection to the cache and to insert entries
// into it) and interactions with the cache (to open it and read from it) take
// place on a background sequence.
class PersistentCodeCacheHost : public CodeCacheHost,
                                public mojom::blink::CodeCacheHost {
 public:
  explicit PersistentCodeCacheHost(
      mojo::Remote<mojom::blink::CodeCacheHost> remote);
  ~PersistentCodeCacheHost() override;

  // CodeCacheHost:
  mojo_base::BigBuffer FetchInlineScriptCacheSync(
      const ParkableString& script_source) override;
  base::WeakPtr<::blink::CodeCacheHost> GetWeakPtr() override;
  mojom::blink::CodeCacheHost* get() override;
  mojom::blink::CodeCacheHost& operator*() override;
  mojom::blink::CodeCacheHost* operator->() override;

  // mojom::blink::CodeCacheHost:
  void GetPendingBackend(mojom::blink::CodeCacheType cache_type,
                         GetPendingBackendCallback callback) override;
  void DidGenerateCacheableMetadata(mojom::blink::CodeCacheType cache_type,
                                    const ::blink::KURL& url,
                                    ::base::Time expected_response_time,
                                    ::mojo_base::BigBuffer data) override;
  void DidGenerateSourceKeyedCacheableMetadata(
      const Vector<uint8_t>& script_hash,
      mojo_base::BigBuffer data) override;
  void FetchCachedCode(mojom::blink::CodeCacheType cache_type,
                       const ::blink::KURL& url,
                       FetchCachedCodeCallback callback) override;
  void ClearCodeCacheEntry(mojom::blink::CodeCacheType cache_type,
                           const ::blink::KURL& url) override;
  void DidGenerateCacheableMetadataInCacheStorage(
      const ::blink::KURL& url,
      ::base::Time expected_response_time,
      ::mojo_base::BigBuffer data,
      const ::blink::String& cache_storage_cache_name) override;

 private:
  class AsyncCodeCacheHost;
  class InlineScriptCacheFetcher;

  void OnFetchCachedCodeReply(FetchCachedCodeCallback callback,
                              base::Time response_time,
                              mojo_base::BigBuffer data);

  SequenceBound<AsyncCodeCacheHost> async_host_;
  base::WeakPtrFactory<PersistentCodeCacheHost> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_PERSISTENT_CODE_CACHE_HOST_H_
