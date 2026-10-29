// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/resources.h"

#include <dlfcn.h>

#include "base/apple/bundle_locations.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
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

    // If running from a bundle, strip "Contents/MacOS/<binary>" to point to the
    // plugin's bundle instead of the hosting app bundle. For unbundled binaries
    // (such as remoting_unittests), point to the generated remoting/resources
    // directory containing the .lproj locale bundles.
    base::FilePath module_dir =
        base::MakeAbsoluteFilePath(base::FilePath(info.dli_fname)).DirName();
    base::FilePath path = module_dir.BaseName().value() == "MacOS"
                              ? module_dir.DirName().DirName()
                              : module_dir.AppendASCII("remoting/resources");
    base::apple::SetOverrideFrameworkBundlePath(path);

    // Override the locale with the value from Cocoa.
    if (pref_locale.empty()) {
      l10n_util::OverrideLocaleWithCocoaLocale();
    }

    ui::ResourceBundle::InitSharedInstanceWithLocale(
        pref_locale, nullptr, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  return true;
}

void UnloadResources() {
  ui::ResourceBundle::CleanupSharedInstance();
  base::apple::SetOverrideFrameworkBundlePath({});
}

}  // namespace remoting
