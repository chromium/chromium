// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/loader/fetch/persistent_code_cache_host.h"
#include "third_party/blink/renderer/platform/loader/fetch/proxy_code_cache_host.h"

namespace blink {

// static
std::unique_ptr<CodeCacheHost> CodeCacheHost::Create(
    mojo::Remote<mojom::blink::CodeCacheHost> remote) {
  if (features::IsPersistentCacheForCodeCacheEnabled()) {
    return std::make_unique<PersistentCodeCacheHost>(std::move(remote));
  }
  return std::make_unique<ProxyCodeCacheHost>(std::move(remote));
}

}  // namespace blink
