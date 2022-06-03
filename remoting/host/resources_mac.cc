// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/resources.h"

#include <dlfcn.h>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/mac/bundle_locations.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

namespace remoting {

bool LoadResources(const std::string& pref_locale) {
  if (ui::ResourceBundle::HasSharedInstance()) {
    ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources(pref_locale);
  } else {
    // Retrieve the path to the module containing this function.
    Dl_info info;
    CHECK(dladdr(reinterpret_cast<void*>(&LoadResources), &info) != 0);

    // Use the plugin's bundle instead of the hosting app bundle. The three
    // DirName() calls strip "Contents/MacOS/<binary>" from the path.
    base::FilePath path =
        base::FilePath(info.dli_fname).DirName().DirName().DirName();
    base::mac::SetOverrideFrameworkBundlePath(path);

    // Override the locale with the value from Cocoa.
    if (pref_locale.empty())
      l10n_util::OverrideLocaleWithCocoaLocale();

    ui::ResourceBundle::InitSharedInstanceWithLocale(
        pref_locale, NULL, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  return true;
}

void UnloadResources() {
  ui::ResourceBundle::CleanupSharedInstance();
}

}  // namespace remoting
