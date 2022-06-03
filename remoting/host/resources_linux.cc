// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/resources.h"

#include <dlfcn.h>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace remoting {

namespace {
const char kLocaleResourcesDirName[] = "remoting_locales";
}  // namespace

bool LoadResources(const std::string& pref_locale) {
  if (ui::ResourceBundle::HasSharedInstance()) {
    ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources(pref_locale);
  } else {
    // Retrieve the path to the module containing this function.
    Dl_info info;
    CHECK(dladdr(reinterpret_cast<void*>(&LoadResources), &info) != 0);

    // Point DIR_LOCALES to 'remoting_locales'.
    base::FilePath path = base::FilePath(info.dli_fname).DirName();
    base::PathService::Override(ui::DIR_LOCALES,
                                path.AppendASCII(kLocaleResourcesDirName));

    ui::ResourceBundle::InitSharedInstanceWithLocale(
        pref_locale, NULL, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  return true;
}

void UnloadResources() {
  ui::ResourceBundle::CleanupSharedInstance();
}

}  // namespace remoting
