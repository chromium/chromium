// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/proxy_code_cache_host.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"

namespace blink {

ProxyCodeCacheHost::ProxyCodeCacheHost(
    mojo::Remote<mojom::blink::CodeCacheHost> remote)
    : remote_(std::move(remote)) {
  DCHECK(remote_.is_bound());
}

ProxyCodeCacheHost::~ProxyCodeCacheHost() = default;

mojo_base::BigBuffer ProxyCodeCacheHost::FetchInlineScriptCacheSync(
    const ParkableString&) {
  // Source-keyed code cache is currently only for inline script cache, which
  // must be handled by `PersistentCodeCacheHost`.
  NOTREACHED();
}

base::WeakPtr<CodeCacheHost> ProxyCodeCacheHost::GetWeakPtr() {
  DCHECK(remote_.is_bound());
  return weak_factory_.GetWeakPtr();
}

mojom::blink::CodeCacheHost* ProxyCodeCacheHost::get() {
  return remote_.get();
}

mojom::blink::CodeCacheHost& ProxyCodeCacheHost::operator*() {
  return *remote_.get();
}

mojom::blink::CodeCacheHost* ProxyCodeCacheHost::operator->() {
  return remote_.get();
}

}  // namespace blink
