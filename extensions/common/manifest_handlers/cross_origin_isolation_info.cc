// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/cross_origin_isolation_info.h"

#include <memory>
#include <utility>

#include "extensions/common/api/cross_origin_isolation.h"

namespace extensions {
namespace {

using COIManifestKeys = api::cross_origin_isolation::ManifestKeys;

const std::string* GetHeaderValue(const Extension& extension, const char* key) {
  CrossOriginIsolationHeader* header =
      static_cast<CrossOriginIsolationHeader*>(extension.GetManifestData(key));
  return header ? &header->value : nullptr;
}

}  // namespace

CrossOriginIsolationHeader::CrossOriginIsolationHeader(std::string value)
    : value(std::move(value)) {}
CrossOriginIsolationHeader::~CrossOriginIsolationHeader() = default;

// static
const std::string* CrossOriginIsolationHeader::GetCrossOriginEmbedderPolicy(
    const Extension& extension) {
  return GetHeaderValue(extension, COIManifestKeys::kCrossOriginEmbedderPolicy);
}

// static
const std::string* CrossOriginIsolationHeader::GetCrossOriginOpenerPolicy(
    const Extension& extension) {
  return GetHeaderValue(extension, COIManifestKeys::kCrossOriginOpenerPolicy);
}

CrossOriginIsolationHandler::CrossOriginIsolationHandler() = default;
CrossOriginIsolationHandler::~CrossOriginIsolationHandler() = default;

bool CrossOriginIsolationHandler::Parse(Extension* extension,
                                        std::u16string* error) {
  COIManifestKeys manifest_keys;
  if (!COIManifestKeys::ParseFromDictionary(
          extension->manifest()->available_values(), manifest_keys, *error)) {
    return false;
  }

  if (manifest_keys.cross_origin_embedder_policy &&
      manifest_keys.cross_origin_embedder_policy->value) {
    extension->SetManifestData(
        COIManifestKeys::kCrossOriginEmbedderPolicy,
        std::make_unique<CrossOriginIsolationHeader>(
            std::move(*manifest_keys.cross_origin_embedder_policy->value)));
  }

  if (manifest_keys.cross_origin_opener_policy &&
      manifest_keys.cross_origin_opener_policy->value) {
    extension->SetManifestData(
        COIManifestKeys::kCrossOriginOpenerPolicy,
        std::make_unique<CrossOriginIsolationHeader>(
            std::move(*manifest_keys.cross_origin_opener_policy->value)));
  }

  return true;
}

base::span<const char* const> CrossOriginIsolationHandler::Keys() const {
  static constexpr const char* kKeys[] = {
      COIManifestKeys::kCrossOriginEmbedderPolicy,
      COIManifestKeys::kCrossOriginOpenerPolicy};
  return kKeys;
}

}  // namespace extensions
