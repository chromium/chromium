// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_PROXY_CODE_CACHE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_PROXY_CODE_CACHE_HOST_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/wtf/sequence_bound.h"

namespace blink {

// Implementation that delegates standard code cache operations to the remote
// instance of `LocalCodeCacheHost` handled in the browser process. When
// InlineScriptCache is enabled, it additionally manages a private background
// reader helper (`SourceKeyedCacheReader`) to serve synchronous source-keyed
// fetch requests locally over shared memory. This implementation is not used
// when `kUsePersistentCacheForCodeCache` is enabled.
class ProxyCodeCacheHost : public CodeCacheHost {
 public:
  explicit ProxyCodeCacheHost(mojo::Remote<mojom::blink::CodeCacheHost> remote);
  ~ProxyCodeCacheHost() override;

  // CodeCacheHost:
  mojo_base::BigBuffer FetchInlineScriptCacheSync(
      const ParkableString& script_source) override;
  base::WeakPtr<CodeCacheHost> GetWeakPtr() override;
  mojom::blink::CodeCacheHost* get() override;
  mojom::blink::CodeCacheHost& operator*() override;
  mojom::blink::CodeCacheHost* operator->() override;

 private:
#if !BUILDFLAG(IS_FUCHSIA)
  class SourceKeyedCacheReader;
  void OnPendingBackend(
      std::optional<persistent_cache::PendingBackend> pending_backend);

  SequenceBound<SourceKeyedCacheReader> reader_;
#endif

  mojo::Remote<mojom::blink::CodeCacheHost> remote_;
  base::WeakPtrFactory<ProxyCodeCacheHost> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_PROXY_CODE_CACHE_HOST_H_
