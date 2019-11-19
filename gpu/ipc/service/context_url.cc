// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/context_url.h"

#include <utility>

#include "base/hash/hash.h"
#include "components/crash/core/common/crash_key.h"

namespace gpu {

// static
void ContextUrl::SetActiveUrl(const gpu::ContextUrl& active_url) {
  // Skip setting crash key when URL hash hasn't changed.
  static size_t last_url_hash = 0;
  if (active_url.hash() == last_url_hash)
    return;

  last_url_hash = active_url.hash();

  // Note that the url is intentionally excluded from webview crash dumps
  // using a whitelist for privacy reasons. See kWebViewCrashKeyWhiteList.
  static crash_reporter::CrashKeyString<1024> crash_key("url-chunk");
  crash_key.Set(active_url.url().possibly_invalid_spec());
}

ContextUrl::ContextUrl(GURL url)
    : url_(std::move(url)),
      url_hash_(base::Hash(url_.possibly_invalid_spec())) {}

}  // namespace gpu
