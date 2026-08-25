// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_config_map.h"

#include <utility>

#include "base/check.h"
#include "base/containers/map_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions {

ExtensionConfigProvider::ExtensionConfigProvider(ExtensionId extension_id)
    : extension_id_(std::move(extension_id)) {}

ExtensionConfigProvider::~ExtensionConfigProvider() = default;

bool ExtensionConfigProvider::IsJsErrorReportingEnabled() const {
  return false;
}

bool ExtensionConfigProvider::ShouldCrashOnJsErrorInDevelopmentBuild() const {
  return false;
}

ExtensionConfigMap::ExtensionConfigMap() = default;

ExtensionConfigMap::~ExtensionConfigMap() = default;

void ExtensionConfigMap::RegisterConfigProvider(
    std::unique_ptr<ExtensionConfigProvider> provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(provider);
  ExtensionId extension_id = provider->extension_id();
  auto [it, inserted] =
      providers_.insert({std::move(extension_id), std::move(provider)});
  CHECK(inserted) << "A config provider for component extension '" << it->first
                  << "' is already registered.";
}

const ExtensionConfigProvider* ExtensionConfigMap::GetConfigProvider(
    const Extension& extension) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!Manifest::IsComponentLocation(extension.location())) {
    return nullptr;
  }
  return base::FindPtrOrNull(providers_, extension.id());
}

void ExtensionConfigMap::ClearProvidersForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  providers_.clear();
}

}  // namespace extensions
