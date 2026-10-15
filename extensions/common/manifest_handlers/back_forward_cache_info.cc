// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/back_forward_cache_info.h"

#include <memory>
#include <optional>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

namespace extensions {

namespace keys = manifest_keys;

BackForwardCacheInfo::BackForwardCacheInfo(bool is_supported)
    : supported(is_supported) {}

BackForwardCacheInfo::~BackForwardCacheInfo() = default;

// static
bool BackForwardCacheInfo::IsSupported(const Extension& extension) {
  const auto* info =
      extension.GetManifestData<BackForwardCacheInfo>(keys::kBackForwardCache);
  return info && info->supported;
}

BackForwardCacheHandler::BackForwardCacheHandler() = default;
BackForwardCacheHandler::~BackForwardCacheHandler() = default;

bool BackForwardCacheHandler::Parse(Extension* extension,
                                    std::u16string* error) {
  CHECK_EQ(mojom::ManifestLocation::kComponent, extension->location());
  const base::Value* value =
      extension->manifest()->FindKey(keys::kBackForwardCache);
  CHECK(value);

  const base::DictValue* dict = value->GetIfDict();
  if (!dict) {
    *error = manifest_errors::kInvalidBackForwardCache;
    return false;
  }

  std::optional<bool> supported_opt = dict->FindBool("supported");
  if (!supported_opt.has_value()) {
    *error = manifest_errors::kInvalidBackForwardCache;
    return false;
  }

  extension->SetManifestData(
      keys::kBackForwardCache,
      std::make_unique<BackForwardCacheInfo>(*supported_opt));
  return true;
}

base::span<const char* const> BackForwardCacheHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kBackForwardCache};
  return kKeys;
}

}  // namespace extensions
