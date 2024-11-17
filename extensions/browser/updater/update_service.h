// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_
#define EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/update_client/update_client.h"
#include "extensions/browser/updater/extension_update_data.h"
#include "extensions/common/extension_id.h"

namespace base {
class Version;
}

namespace content {
class BrowserContext;
}

namespace update_client {
enum class Error;
struct CrxUpdateItem;
class UpdateClient;
}

namespace extensions {

class ExtensionUpdateClientBaseTest;
struct ExtensionUpdateCheckParams;
class UpdateDataProvider;
class UpdateServiceFactory;

// An UpdateService provides functionality to update extensions.
// Some methods are virtual for testing purposes.
class UpdateService : public KeyedService {
 public:
  UpdateService(const UpdateService&) = delete;
  UpdateService& operator=(const UpdateService&) = delete;

  static UpdateService* Get(content::BrowserContext* context);

  static void SupplyUpdateServiceForTest(UpdateService* service);

  void Shutdown() override;

  virtual void SendUninstallPing(const std::string& id,
                                 const base::Version& version,
                                 int reason);

  // Starts an update check for each of extensions stored in |update_params|.
  // If there are any updates available, they will be downloaded, checked for
  // integrity, unpacked, and then passed off to the
  // ExtensionSystem::InstallUpdate method for install completion.
  virtual void StartUpdateCheck(const ExtensionUpdateCheckParams& update_params,
                                UpdateFoundCallback update_found_callback,
                                base::OnceClosure callback);

  UpdateService(content::BrowserContext* context,
                scoped_refptr<update_client::UpdateClient> update_client);
  ~UpdateService() override;

 private:
  friend class ExtensionUpdateClientBaseTest;
  friend class UpdateServiceFactory;
  friend std::unique_ptr<UpdateService>::deleter_type;

  struct InProgressUpdate {
    InProgressUpdate(base::OnceClosure callback, bool install_immediately);
    ~InProgressUpdate();

    InProgressUpdate(const InProgressUpdate& other) = delete;
    InProgressUpdate& operator=(const InProgressUpdate& other) = delete;

    InProgressUpdate(InProgressUpdate&& other);
    InProgressUpdate& operator=(InProgressUpdate&& other);

    base::OnceClosure callback;
    bool install_immediately;
    std::set<std::string> pending_extension_ids;
  };

  // This function is executed by the update client after an update check
  // request has completed.
  void UpdateCheckComplete(InProgressUpdate update);

  // Adds/Removes observer to/from |update_client::UpdateClient|.
  // Used by browser tests.
  void AddUpdateClientObserver(update_client::UpdateClient::Observer* observer);
  void RemoveUpdateClientObserver(
      update_client::UpdateClient::Observer* observer);

  void OnCrxStateChange(UpdateFoundCallback update_found_callback,
                        const update_client::CrxUpdateItem& item);

  void HandleComponentUpdateErrorEvent(const ExtensionId& extension_id) const;

  // Get the extension Omaha attributes sent from update config.
  base::Value::Dict GetExtensionOmahaAttributes(
      const update_client::CrxUpdateItem& update_item);

 private:
  raw_ptr<content::BrowserContext> browser_context_;

  scoped_refptr<update_client::UpdateClient> update_client_;
  scoped_refptr<UpdateDataProvider> update_data_provider_;

  THREAD_CHECKER(thread_checker_);

  // used to create WeakPtrs to |this|.
  base::WeakPtrFactory<UpdateService> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_
