// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_UPDATE_DATA_PROVIDER_H_
#define EXTENSIONS_BROWSER_UPDATER_UPDATE_DATA_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/updater/extension_installer.h"
#include "extensions/browser/updater/extension_update_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace update_client {
struct CrxComponent;
}

namespace extensions {

// This class exists to let an UpdateClient retrieve information about a set of
// extensions it is doing an update check for.
class UpdateDataProvider : public base::RefCounted<UpdateDataProvider> {
 public:
  using UpdateClientCallback = ExtensionInstaller::UpdateClientCallback;

  // We need a browser context to use when retrieving data for a set of
  // extension ids, as well as an install callback for proceeding with
  // installation steps once the UpdateClient has downloaded and unpacked
  // an update for an extension.
  explicit UpdateDataProvider(content::BrowserContext* browser_context);

  UpdateDataProvider(const UpdateDataProvider&) = delete;
  UpdateDataProvider& operator=(const UpdateDataProvider&) = delete;

  // Notify this object that the associated browser context is being shut down
  // the pointer to the context should be dropped and no more work should be
  // done.
  void Shutdown();

  std::vector<absl::optional<update_client::CrxComponent>> GetData(
      bool install_immediately,
      const ExtensionUpdateDataMap& update_info,
      const std::vector<std::string>& ids);

 private:
  friend class base::RefCounted<UpdateDataProvider>;
  ~UpdateDataProvider();

  // This function should be called on the browser UI thread.
  void RunInstallCallback(const std::string& extension_id,
                          const std::string& public_key,
                          const base::FilePath& unpacked_dir,
                          bool install_immediately,
                          UpdateClientCallback update_client_callback);

  void InstallUpdateCallback(const std::string& extension_id,
                             const std::string& public_key,
                             const base::FilePath& unpacked_dir,
                             bool install_immediately,
                             UpdateClientCallback update_client_callback);

  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_UPDATE_DATA_PROVIDER_H_
