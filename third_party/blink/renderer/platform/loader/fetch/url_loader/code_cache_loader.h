// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_CODE_CACHE_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_CODE_CACHE_LOADER_H_

#include "third_party/blink/public/platform/web_code_cache_loader.h"

namespace blink {

// This class is loading V8 compilation code cache for scripts
// (either separate script resources, or inline scripts in html file).
// It is talking to the browser process and uses per-site isolated
// cache backend to avoid cross-origin contamination.
class BLINK_PLATFORM_EXPORT CodeCacheLoader : public WebCodeCacheLoader {
 public:
  // |code_cache_host| is the per-frame mojo interface that should be used when
  // fetching code cache. If this value is nullptr it uses per-process
  // interface.
  // TODO(mythria): Remove the per-process interface and only expect non nullptr
  // for |code_cache_host|.
  explicit CodeCacheLoader(mojom::CodeCacheHost* code_cache_host);

  ~CodeCacheLoader() override;

  void FetchFromCodeCache(mojom::CodeCacheType cache_type,
                          const WebURL& url,
                          FetchCodeCacheCallback callback) override;

 private:
  mojom::CodeCacheHost* const code_cache_host_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER__FETCH_URL_LOADER_CODE_CACHE_LOADER_H_
