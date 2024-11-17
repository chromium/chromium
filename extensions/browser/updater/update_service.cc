// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_service.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_update_data.h"
#include "extensions/browser/updater/scoped_extension_updater_keep_alive.h"
#include "extensions/browser/updater/update_data_provider.h"
#include "extensions/browser/updater/update_service_factory.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace {

UpdateService* update_service_override = nullptr;

// This set contains all Omaha attributes that is associated with extensions.
constexpr const char* kOmahaAttributes[] = {
    "_malware", "_esbAllowlist", "_potentially_uws", "_policy_violation"};

}  // namespace

UpdateService::InProgressUpdate::InProgressUpdate(base::OnceClosure callback,
                                                  bool install_immediately)
    : callback(std::move(callback)), install_immediately(install_immediately) {}
UpdateService::InProgressUpdate::~InProgressUpdate() = default;

UpdateService::InProgressUpdate::InProgressUpdate(
    UpdateService::InProgressUpdate&& other) = default;
UpdateService::InProgressUpdate& UpdateService::InProgressUpdate::operator=(
    UpdateService::InProgressUpdate&& other) = default;

// static
void UpdateService::SupplyUpdateServiceForTest(UpdateService* service) {
  update_service_override = service;
}

// static
UpdateService* UpdateService::Get(content::BrowserContext* context) {
  return update_service_override == nullptr
             ? UpdateServiceFactory::GetForBrowserContext(context)
             : update_service_override;
}

void UpdateService::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (update_data_provider_) {
    update_data_provider_->Shutdown();
    update_data_provider_ = nullptr;
  }
  update_client_ = nullptr;
  browser_context_ = nullptr;
}

void UpdateService::SendUninstallPing(const std::string& id,
                                      const base::Version& version,
                                      int reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(update_client_);
  update_client::CrxComponent crx;
  crx.app_id = id;
  crx.version = version;
  // A ScopedExtensionUpdaterKeepAlive is bound into the callback to keep the
  // context alive throughout the operation.
  update_client_->SendPing(
      crx,
      {.event_type = update_client::protocol_request::kEventUninstall,
       .result = 1,
       .error_code = 0,
       .extra_code1 = reason},
      base::BindOnce([](std::unique_ptr<ScopedExtensionUpdaterKeepAlive>,
                        update_client::Error) {},
                     ExtensionsBrowserClient::Get()->CreateUpdaterKeepAlive(
                         browser_context_)));
}

void UpdateService::OnCrxStateChange(UpdateFoundCallback update_found_callback,
                                     const update_client::CrxUpdateItem& item) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Custom attributes can only be sent for NOT_UPDATED/UPDATE_FOUND events.
  bool should_perform_action_on_omaha_attributes = false;

  switch (item.state) {
    case update_client::ComponentState::kUpToDate:
      should_perform_action_on_omaha_attributes = true;
      break;
    case update_client::ComponentState::kCanUpdate:
      should_perform_action_on_omaha_attributes = true;
      if (update_found_callback) {
        update_found_callback.Run(item.id, item.next_version);
      }
      break;
    case update_client::ComponentState::kNew:
    case update_client::ComponentState::kChecking:
    case update_client::ComponentState::kDownloadingDiff:
    case update_client::ComponentState::kDownloading:
    case update_client::ComponentState::kUpdatingDiff:
    case update_client::ComponentState::kUpdating:
    case update_client::ComponentState::kUpdated:
    case update_client::ComponentState::kUpdateError:
    case update_client::ComponentState::kRun:
    case update_client::ComponentState::kLastStatus:
      break;
  }

  if (should_perform_action_on_omaha_attributes) {
    base::Value::Dict attributes = GetExtensionOmahaAttributes(item);
    // Note that it's important to perform actions even if |attributes| is
    // empty, missing values may default to false and have associated logic.
    ExtensionSystem::Get(browser_context_)
        ->PerformActionBasedOnOmahaAttributes(item.id, attributes);
  }
}

UpdateService::UpdateService(
    content::BrowserContext* browser_context,
    scoped_refptr<update_client::UpdateClient> update_client)
    : browser_context_(browser_context), update_client_(update_client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  update_data_provider_ =
      base::MakeRefCounted<UpdateDataProvider>(browser_context_);
}

UpdateService::~UpdateService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void UpdateService::StartUpdateCheck(
    const ExtensionUpdateCheckParams& update_params,
    UpdateFoundCallback update_found_callback,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!update_params.update_info.empty());

  VLOG(2) << "UpdateService::StartUpdateCheck";

  if (!ExtensionsBrowserClient::Get()->IsBackgroundUpdateAllowed()) {
    VLOG(1) << "UpdateService - Extension update not allowed.";
    if (!callback.is_null()) {
      std::move(callback).Run();
    }
    return;
  }

  InProgressUpdate update =
      InProgressUpdate(std::move(callback), update_params.install_immediately);

  ExtensionUpdateDataMap update_data;
  std::vector<std::vector<ExtensionId>> update_ids;
  update_ids.reserve(update_params.update_info.size());
  for (const auto& update_info : update_params.update_info) {
    const ExtensionId& extension_id = update_info.first;

    DCHECK(!extension_id.empty());

    update.pending_extension_ids.insert(extension_id);

    ExtensionUpdateData data = update_info.second;
    if (data.is_corrupt_reinstall) {
      data.install_source = "reinstall";
    } else if (data.install_source.empty() &&
               update_params.priority ==
                   ExtensionUpdateCheckParams::FOREGROUND) {
      data.install_source = "ondemand";
    }
    if (update_ids.empty() || update_ids.back().size() >= 25) {
      update_ids.emplace_back();
    }
    update_ids.back().push_back(extension_id);
    update_data.insert(std::make_pair(extension_id, data));
  }

  base::RepeatingCallback closure = base::BarrierClosure(
      update_ids.size(),
      base::BindOnce(&UpdateService::UpdateCheckComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(update)));

  base::RepeatingCallback<void(
      const std::vector<std::string>&,
      base::OnceCallback<void(
          const std::vector<std::optional<update_client::CrxComponent>>&)>)>
      get_data = base::BindRepeating(
          &UpdateDataProvider::GetData, update_data_provider_,
          update_params.install_immediately, std::move(update_data));

  for (const std::vector<std::string>& update_id_group : update_ids) {
    update_client_->Update(
        update_id_group, get_data,
        base::BindRepeating(&UpdateService::OnCrxStateChange,
                            weak_ptr_factory_.GetWeakPtr(),
                            update_found_callback),
        update_params.priority == ExtensionUpdateCheckParams::FOREGROUND,
        base::BindOnce([](base::RepeatingClosure callback,
                          update_client::Error /*error*/) { callback.Run(); },
                       closure));
  }
}

void UpdateService::UpdateCheckComplete(InProgressUpdate update) {
  VLOG(2) << "UpdateService::UpdateCheckComplete";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // When no update is found or there's an update error, a previous update
  // check might have queued an update for this extension because it was in
  // use at the time. We should ask for the install of the queued update now
  // if it's ready.
  if (update.install_immediately) {
    for (const ExtensionId& extension_id : update.pending_extension_ids) {
      ExtensionSystem::Get(browser_context_)
          ->FinishDelayedInstallationIfReady(extension_id,
                                             true /*install_immediately*/);
    }
  }

  if (!update.callback.is_null()) {
    std::move(update.callback).Run();
  }
}

void UpdateService::AddUpdateClientObserver(
    update_client::UpdateClient::Observer* observer) {
  if (update_client_) {
    update_client_->AddObserver(observer);
  }
}

void UpdateService::RemoveUpdateClientObserver(
    update_client::UpdateClient::Observer* observer) {
  if (update_client_) {
    update_client_->RemoveObserver(observer);
  }
}

base::Value::Dict UpdateService::GetExtensionOmahaAttributes(
    const update_client::CrxUpdateItem& update_item) {
  base::Value::Dict attributes;

  for (const char* key : kOmahaAttributes) {
    auto iter = update_item.custom_updatecheck_data.find(key);
    // This is assuming that the values of the keys are "true", "false",
    // or does not exist.
    // Only create the attribute if it's defined in the custom update check
    // data. We want to distinguish true, false and undefined values.
    if (iter != update_item.custom_updatecheck_data.end()) {
      attributes.Set(key, iter->second == "true");
    }
  }
  return attributes;
}
}  // namespace extensions
