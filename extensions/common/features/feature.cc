// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature.h"

#include <map>
#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions {

// static
Feature::Platform Feature::GetCurrentPlatform() {
#if BUILDFLAG(IS_CHROMEOS)
  return CHROMEOS_PLATFORM;
#elif BUILDFLAG(IS_LINUX)
  return LINUX_PLATFORM;
#elif BUILDFLAG(IS_MAC)
  return MACOSX_PLATFORM;
#elif BUILDFLAG(IS_WIN)
  return WIN_PLATFORM;
#elif BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  return DESKTOP_ANDROID_PLATFORM;
#else
  return UNSPECIFIED_PLATFORM;
#endif
}

Feature::Availability Feature::IsAvailableToExtension(
    const Extension* extension) const {
  return IsAvailableToManifest(
      extension->hashed_id(), extension->GetType(), extension->location(),
      extension->manifest_version(), kUnspecifiedContextId);
}

Feature::Feature(const FeatureData* feature_data)
    : feature_data_(feature_data) {
  CHECK(feature_data_);
}

Feature::~Feature() = default;

bool Feature::HasDelegatedAvailabilityCheckHandlerForTesting() const {
  return HasDelegatedAvailabilityCheckHandler();
}

}  // namespace extensions
