// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/feature.h"

#include <map>
#include <string_view>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions {

// static
Feature::Platform Feature::GetCurrentPlatform() {
// TODO(crbug.com/40118868): For readability, this should become
// BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(IS_CHROMEOS_LACROS). The second
// conditional should be BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(IS_CHROMEOS_ASH).
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return LACROS_PLATFORM;
#elif BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  return CHROMEOS_PLATFORM;
#elif BUILDFLAG(IS_LINUX)
  return LINUX_PLATFORM;
#elif BUILDFLAG(IS_MAC)
  return MACOSX_PLATFORM;
#elif BUILDFLAG(IS_WIN)
  return WIN_PLATFORM;
#elif BUILDFLAG(IS_FUCHSIA)
  return FUCHSIA_PLATFORM;
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

Feature::Feature() : no_parent_(false) {}

Feature::~Feature() = default;

void Feature::set_name(std::string_view name) {
  name_ = std::string(name);
}

void Feature::set_alias(std::string_view alias) {
  alias_ = std::string(alias);
}

void Feature::set_source(std::string_view source) {
  source_ = std::string(source);
}

bool Feature::HasDelegatedAvailabilityCheckHandlerForTesting() const {
  return HasDelegatedAvailabilityCheckHandler();
}

}  // namespace extensions
