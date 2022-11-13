// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/code_cache_loader.h"

#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/loader/fetch/code_cache_host.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

CodeCacheLoader::CodeCacheLoader(CodeCacheHost* code_cache_host) {
  DCHECK(code_cache_host);
  code_cache_host_ = code_cache_host->GetWeakPtr();
}

CodeCacheLoader::~CodeCacheLoader() = default;

void CodeCacheLoader::FetchFromCodeCache(mojom::CodeCacheType cache_type,
                                         const WebURL& url,
                                         FetchCodeCacheCallback callback) {
  DCHECK(code_cache_host_);
  code_cache_host_->get()->FetchCachedCode(cache_type, url,
                                           std::move(callback));
}

void CodeCacheLoader::ClearCodeCacheEntry(mojom::CodeCacheType cache_type,
                                          const WebURL& url) {
  DCHECK(code_cache_host_);
  code_cache_host_->get()->ClearCodeCacheEntry(cache_type, url);
}

// static
std::unique_ptr<WebCodeCacheLoader> WebCodeCacheLoader::Create(
    CodeCacheHost* code_cache_host) {
  return std::make_unique<CodeCacheLoader>(code_cache_host);
}

}  // namespace blink
