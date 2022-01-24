// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/code_cache_loader.h"

#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "url/gurl.h"

namespace blink {

CodeCacheLoader::CodeCacheLoader(CodeCacheHost* code_cache_host)
    : code_cache_host_(code_cache_host ? code_cache_host->GetWeakPtr()
                                       : nullptr) {}

CodeCacheLoader::~CodeCacheLoader() = default;

void CodeCacheLoader::FetchFromCodeCache(mojom::CodeCacheType cache_type,
                                         const WebURL& url,
                                         FetchCodeCacheCallback callback) {
  if (code_cache_host_) {
    code_cache_host_->get()->FetchCachedCode(
        cache_type, static_cast<GURL>(static_cast<KURL>(url)),
        std::move(callback));
  } else if (ShouldUsePerProcessInterface()) {
    // TODO(mythria): This path is required for workers currently. Once we
    // update worker requests to go through WorkerHost remove this path.
    Platform::Current()->FetchCachedCode(cache_type, url, std::move(callback));
  }
}

void CodeCacheLoader::ClearCodeCacheEntry(mojom::CodeCacheType cache_type,
                                          const WebURL& url) {
  if (code_cache_host_) {
    code_cache_host_->get()->ClearCodeCacheEntry(
        cache_type, static_cast<GURL>(static_cast<KURL>(url)));
  } else if (ShouldUsePerProcessInterface()) {
    // TODO(mythria): This path is required for worklets currently. Once we
    // update worker requests to go through WorkerHost remove this path.
    Platform::Current()->ClearCodeCacheEntry(
        cache_type, static_cast<GURL>(static_cast<KURL>(url)));
  }
}

bool CodeCacheLoader::ShouldUsePerProcessInterface() const {
  // If the code cache host is nullptr, and was never invalidated, then it was
  // initialised with nullptr. In this case, we should use the per-process
  // interface. Otherwise, we should try to use the host.
  return !code_cache_host_ && !code_cache_host_.WasInvalidated();
}

// static
std::unique_ptr<WebCodeCacheLoader> WebCodeCacheLoader::Create(
    CodeCacheHost* code_cache_host) {
  return std::make_unique<CodeCacheLoader>(code_cache_host);
}

}  // namespace blink
