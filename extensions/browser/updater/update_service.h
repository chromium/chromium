// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_
#define EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/update_client/update_client.h"

namespace base {
class Version;
}

namespace content {
class BrowserContext;
}

namespace update_client {
enum class Error;
class UpdateClient;
}

namespace extensions {

class ExtensionUpdateClientBaseTest;
struct ExtensionUpdateCheckParams;
class UpdateDataProvider;
class UpdateServiceFactory;

// An UpdateService provides functionality to update extensions.
// Some methods are virtual for testing purposes.
class UpdateService : public KeyedService,
                      update_client::UpdateClient::Observer {
 public:
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
                                base::OnceClosure callback);

  // This function verifies if the current implementation can update
  // |extension_id|.
  virtual bool CanUpdate(const std::string& extension_id) const;

  // Overriden from |update_client::UpdateClient::Observer|.
  void OnEvent(Events event, const std::string& id) override;

  // Returns true if the update service is updating one or more extensions.
  virtual bool IsBusy() const;

 protected:
  UpdateService(content::BrowserContext* context,
                scoped_refptr<update_client::UpdateClient> update_client);
  ~UpdateService() override;

 private:
  friend class ExtensionUpdateClientBaseTest;
  friend class UpdateServiceFactory;
  friend std::unique_ptr<UpdateService>::deleter_type;

  // This function is executed by the update client after an update check
  // request has completed.
  void UpdateCheckComplete(update_client::Error error);

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

  // Adds/Removes observer to/from |update_client::UpdateClient|.
  // Mainly used for browser tests.
  void AddUpdateClientObserver(update_client::UpdateClient::Observer* observer);
  void RemoveUpdateClientObserver(
      update_client::UpdateClient::Observer* observer);
  void HandleComponentUpdateErrorEvent(const std::string& extension_id) const;
  void HandleComponentUpdateFoundEvent(const std::string& extension_id) const;

 private:
  content::BrowserContext* browser_context_;

  scoped_refptr<update_client::UpdateClient> update_client_;
  scoped_refptr<UpdateDataProvider> update_data_provider_;

  // The set of extension IDs that are being checked for update.
  std::set<std::string> updating_extension_ids_;
  std::vector<InProgressUpdate> in_progress_updates_;

  THREAD_CHECKER(thread_checker_);

  // used to create WeakPtrs to |this|.
  base::WeakPtrFactory<UpdateService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UpdateService);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_
