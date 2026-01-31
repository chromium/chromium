// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_assets_manager.h"

#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {

class EmptyExtensionAssetsManager : public ExtensionAssetsManager {
 public:
  EmptyExtensionAssetsManager() = default;
  ~EmptyExtensionAssetsManager() override = default;
  EmptyExtensionAssetsManager(const EmptyExtensionAssetsManager&) = delete;
  EmptyExtensionAssetsManager& operator=(const EmptyExtensionAssetsManager&) =
      delete;

  // Override from ExtensionAssetsManager.
  void InstallExtension(
      const Extension* extension,
      const base::FilePath& unpacked_extension_root,
      const base::FilePath& local_install_dir,
      content::BrowserContext* browser_context,
      InstallExtensionCallback callback,
      bool updates_from_webstore_or_empty_update_url) override {
    std::move(callback).Run(file_util::InstallExtension(
        unpacked_extension_root, extension->id(), extension->VersionString(),
        local_install_dir));
  }

  void UninstallExtension(const std::string& id,
                          const std::string& profile_user_name,
                          const base::FilePath& extensions_install_dir,
                          const base::FilePath& extension_dir_to_delete,
                          const base::FilePath& profile_dir) override {
    file_util::UninstallExtension(profile_dir, extensions_install_dir,
                                  extension_dir_to_delete);
  }
};

}  // namespace

// static
std::unique_ptr<ExtensionAssetsManager>
ExtensionAssetsManager::CreateDefaultInstance() {
  return std::make_unique<EmptyExtensionAssetsManager>();
}

}  // namespace extensions
