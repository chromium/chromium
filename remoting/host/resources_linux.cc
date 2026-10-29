// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/resources.h"

#include <dlfcn.h>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace remoting {

namespace {
const char kLocaleResourcesDirName[] = "remoting_locales";
}  // namespace

bool LoadResources(const std::string& pref_locale) {
  // Retrieve the path to the module containing this function.
  Dl_info info;
  CHECK(dladdr(reinterpret_cast<void*>(&LoadResources), &info) != 0);

  // Point DIR_LOCALES to 'remoting_locales'.
  base::FilePath path = base::FilePath(info.dli_fname).DirName();
  base::PathService::Override(ui::DIR_LOCALES,
                              path.AppendASCII(kLocaleResourcesDirName));

  if (ui::ResourceBundle::HasSharedInstance()) {
    ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources(pref_locale);
  } else {
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        pref_locale, nullptr, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  return true;
}

void UnloadResources() {
#if !BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, the shared ResourceBundle is owned by the browser or
  // ChromeOSRemotingTestSuite.
  ui::ResourceBundle::CleanupSharedInstance();
#endif
}

}  // namespace remoting
