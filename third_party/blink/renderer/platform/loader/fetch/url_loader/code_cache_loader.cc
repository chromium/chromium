// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/code_cache_loader.h"

#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "url/gurl.h"

namespace blink {

CodeCacheLoader::CodeCacheLoader(mojom::CodeCacheHost* code_cache_host)
    : code_cache_host_(code_cache_host) {}

CodeCacheLoader::~CodeCacheLoader() = default;

void CodeCacheLoader::FetchFromCodeCache(mojom::CodeCacheType cache_type,
                                         const WebURL& url,
                                         FetchCodeCacheCallback callback) {
  if (code_cache_host_) {
    code_cache_host_->FetchCachedCode(cache_type,
                                      static_cast<GURL>(static_cast<KURL>(url)),
                                      std::move(callback));
  } else {
    // TODO(mythria): This path is required for workers currently. Once we
    // update worker requests to go through WorkerHost remove this path.
    Platform::Current()->FetchCachedCode(cache_type, url, std::move(callback));
  }
}

// static
std::unique_ptr<WebCodeCacheLoader> WebCodeCacheLoader::Create(
    mojom::CodeCacheHost* code_cache_host) {
  return std::make_unique<CodeCacheLoader>(code_cache_host);
}

}  // namespace blink
