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

namespace blink {

// Implementation that delegates to the remote instance. All operations are
// satisfied by `LocalCodeCacheHost` in the browser process. This implementation
// is used when the UsePersistentCacheForCodeCache feature is not enabled.
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
  mojo::Remote<mojom::blink::CodeCacheHost> remote_;
  base::WeakPtrFactory<ProxyCodeCacheHost> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_PROXY_CODE_CACHE_HOST_H_
