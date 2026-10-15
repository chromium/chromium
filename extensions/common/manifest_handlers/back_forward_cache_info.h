// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_BACK_FORWARD_CACHE_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_BACK_FORWARD_CACHE_INFO_H_

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct BackForwardCacheInfo : public Extension::ManifestData {
  explicit BackForwardCacheInfo(bool is_supported);
  ~BackForwardCacheInfo() override;

  bool supported = false;

  // Whether the extension has opted into BackForwardCache support. While
  // currently restricted to component extensions during initial development
  // and vetting, this opt-in key could potentially be opened to broader
  // extensions in the future.
  static bool IsSupported(const Extension& extension);
};

// Parses the "back_forward_cache" manifest key.
class BackForwardCacheHandler : public ManifestHandler {
 public:
  BackForwardCacheHandler();

  BackForwardCacheHandler(const BackForwardCacheHandler&) = delete;
  BackForwardCacheHandler& operator=(const BackForwardCacheHandler&) = delete;

  ~BackForwardCacheHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_BACK_FORWARD_CACHE_INFO_H_
