// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/app_display_info.h"

#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

// This info is used for both the kDisplayInLauncher and kDisplayInNewTabPage
// keys, but we just arbitrarily pick one to store it under in the manifest.
const char* kAppDisplayInfoKey = keys::kDisplayInLauncher;

AppDisplayInfo* GetAppDisplayInfo(const Extension& extension) {
  auto* info = static_cast<AppDisplayInfo*>(
      extension.GetManifestData(kAppDisplayInfoKey));
  DCHECK(!info || extension.is_app())
      << "Only apps are allowed to be displayed in the NTP or launcher.";
  return info;
}

}  // namespace

AppDisplayInfo::AppDisplayInfo(bool display_in_launcher,
                               bool display_in_new_tab_page)
    : display_in_launcher_(display_in_launcher),
      display_in_new_tab_page_(display_in_new_tab_page) {}
AppDisplayInfo::~AppDisplayInfo() = default;

bool AppDisplayInfo::RequiresSortOrdinal(const Extension& extension) {
  AppDisplayInfo* info = GetAppDisplayInfo(extension);
  return info && (info->display_in_launcher_ || info->display_in_new_tab_page_);
}

bool AppDisplayInfo::ShouldDisplayInAppLauncher(const Extension& extension) {
  AppDisplayInfo* info = GetAppDisplayInfo(extension);
  return info && info->display_in_launcher_;
}

bool AppDisplayInfo::ShouldDisplayInNewTabPage(const Extension& extension) {
  AppDisplayInfo* info = GetAppDisplayInfo(extension);
  return info && info->display_in_new_tab_page_;
}

AppDisplayManifestHandler::AppDisplayManifestHandler() = default;
AppDisplayManifestHandler::~AppDisplayManifestHandler() = default;

bool AppDisplayManifestHandler::Parse(Extension* extension,
                                      std::u16string* error) {
  bool display_in_launcher = true;
  bool display_in_new_tab_page = true;

  const Manifest* manifest = extension->manifest();
  if (const base::Value* value = manifest->FindKey(keys::kDisplayInLauncher)) {
    if (!value->is_bool()) {
      *error = errors::kInvalidDisplayInLauncher;
      return false;
    }
    display_in_launcher = value->GetBool();
  }
  if (const base::Value* value =
          manifest->FindKey(keys::kDisplayInNewTabPage)) {
    if (!value->is_bool()) {
      *error = errors::kInvalidDisplayInNewTabPage;
      return false;
    }
    display_in_new_tab_page = value->GetBool();
  } else {
    // Inherit default from display_in_launcher property.
    display_in_new_tab_page = display_in_launcher;
  }

  extension->SetManifestData(kAppDisplayInfoKey,
                             std::make_unique<AppDisplayInfo>(
                                 display_in_launcher, display_in_new_tab_page));
  return true;
}

base::span<const char* const> AppDisplayManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kDisplayInLauncher,
                                          keys::kDisplayInNewTabPage};
  return kKeys;
}

bool AppDisplayManifestHandler::AlwaysParseForType(Manifest::Type type) const {
  // Always parse for app types; this ensures that apps default to being
  // displayed in the proper surfaces.
  return type == Manifest::TYPE_LEGACY_PACKAGED_APP ||
         type == Manifest::TYPE_HOSTED_APP ||
         type == Manifest::TYPE_PLATFORM_APP;
}

}  // namespace extensions
