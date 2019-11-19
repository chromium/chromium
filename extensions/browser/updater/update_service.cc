// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_service.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_update_data.h"
#include "extensions/browser/updater/update_data_provider.h"
#include "extensions/browser/updater/update_service_factory.h"
#include "extensions/common/extension_updater_uma.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_url_handlers.h"

namespace extensions {

namespace {

UpdateService* update_service_override = nullptr;

// 98% of update checks have 22 or less extensions.
constexpr size_t kMaxExtensionsPerUpdate = 22;

void SendUninstallPingCompleteCallback(update_client::Error error) {}

void ReportUpdateCheckResult(ExtensionUpdaterUpdateResult update_result,
                             int error_code) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.ExtensionUpdaterUpdateResults",
                            update_result,
                            ExtensionUpdaterUpdateResult::UPDATE_RESULT_COUNT);

  // This UMA histogram measures update check results of the unified extension
  // updater.
  UMA_HISTOGRAM_ENUMERATION("Extensions.UnifiedExtensionUpdaterUpdateResults",
                            update_result,
                            ExtensionUpdaterUpdateResult::UPDATE_RESULT_COUNT);

  switch (update_result) {
    case ExtensionUpdaterUpdateResult::UPDATE_CHECK_ERROR:
      base::UmaHistogramSparse(
          "Extensions.UnifiedExtensionUpdaterUpdateCheckErrors", error_code);
      break;
    case ExtensionUpdaterUpdateResult::UPDATE_DOWNLOAD_ERROR:
      base::UmaHistogramSparse(
          "Extensions.UnifiedExtensionUpdaterDownloadErrors", error_code);
      break;
    case ExtensionUpdaterUpdateResult::UPDATE_SERVICE_ERROR:
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.UnifiedExtensionUpdaterUpdateServiceErrors",
          static_cast<update_client::Error>(error_code),
          update_client::Error::MAX_VALUE);
      break;
    default:
      break;
  }
}

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
  RemoveUpdateClientObserver(this);
  update_client_ = nullptr;
  browser_context_ = nullptr;
}

void UpdateService::SendUninstallPing(const std::string& id,
                                      const base::Version& version,
                                      int reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(update_client_);
  update_client_->SendUninstallPing(
      id, version, reason, base::BindOnce(&SendUninstallPingCompleteCallback));
}

bool UpdateService::CanUpdate(const std::string& extension_id) const {
  // It's possible to change Webstore update URL from command line (through
  // apps-gallery-update-url command line switch). When Webstore update URL is
  // different the default Webstore update URL, we won't support updating
  // extensions through UpdateService.
  if (extension_urls::GetDefaultWebstoreUpdateUrl() !=
      extension_urls::GetWebstoreUpdateUrl())
    return false;

  // Won't update extensions with empty IDs.
  if (extension_id.empty())
    return false;

  // We can only update extensions that have been installed on the system.
  // Furthermore, we can only update extensions that were installed from the
  // webstore or extensions with empty update URLs not converted from user
  // scripts.
  const ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const Extension* extension = registry->GetInstalledExtension(extension_id);
  if (extension == nullptr)
    return false;
  const GURL& update_url = ManifestURL::GetUpdateURL(extension);
  if (update_url.is_empty())
    return !extension->converted_from_user_script();
  return extension_urls::IsWebstoreUpdateUrl(update_url);
}

void UpdateService::OnEvent(Events event, const std::string& extension_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << "UpdateService::OnEvent " << static_cast<int>(event) << " "
          << extension_id;

  bool complete_event = false;
  bool finish_delayed_installation = false;
  switch (event) {
    case Events::COMPONENT_UPDATED:
      complete_event = true;
      ReportUpdateCheckResult(ExtensionUpdaterUpdateResult::UPDATE_SUCCESS, 0);
      break;
    case Events::COMPONENT_UPDATE_ERROR:
      complete_event = true;
      finish_delayed_installation = true;
      HandleComponentUpdateErrorEvent(extension_id);
      break;
    case Events::COMPONENT_NOT_UPDATED:
      complete_event = true;
      finish_delayed_installation = true;
      ReportUpdateCheckResult(ExtensionUpdaterUpdateResult::NO_UPDATE, 0);
      break;
    case Events::COMPONENT_UPDATE_FOUND:
      HandleComponentUpdateFoundEvent(extension_id);
      break;
    case Events::COMPONENT_CHECKING_FOR_UPDATES:
    case Events::COMPONENT_WAIT:
    case Events::COMPONENT_UPDATE_READY:
    case Events::COMPONENT_UPDATE_DOWNLOADING:
      break;
  }

  if (complete_event) {
    // The update check for |extension_id| has completed, thus it can be
    // removed from all in-progress update checks.
    DCHECK(updating_extension_ids_.count(extension_id) > 0);
    updating_extension_ids_.erase(extension_id);

    bool install_immediately = false;
    for (auto& update : in_progress_updates_) {
      install_immediately |= update.install_immediately;
      update.pending_extension_ids.erase(extension_id);
    }

    // When no update is found or there's an update error, a previous update
    // check might have queued an update for this extension because it was in
    // use at the time. We should ask for the install of the queued update now
    // if it's ready.
    if (finish_delayed_installation && install_immediately) {
      ExtensionSystem::Get(browser_context_)
          ->FinishDelayedInstallationIfReady(extension_id,
                                             true /*install_immediately*/);
    }
  }
}

bool UpdateService::IsBusy() const {
  return !updating_extension_ids_.empty();
}

UpdateService::UpdateService(
    content::BrowserContext* browser_context,
    scoped_refptr<update_client::UpdateClient> update_client)
    : browser_context_(browser_context), update_client_(update_client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  update_data_provider_ =
      base::MakeRefCounted<UpdateDataProvider>(browser_context_);
  AddUpdateClientObserver(this);
}

UpdateService::~UpdateService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (update_client_)
    update_client_->RemoveObserver(this);
}

void UpdateService::StartUpdateCheck(
    const ExtensionUpdateCheckParams& update_params,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!update_params.update_info.empty());

  VLOG(2) << "UpdateService::StartUpdateCheck";

  if (!ExtensionsBrowserClient::Get()->IsBackgroundUpdateAllowed()) {
    VLOG(1) << "UpdateService - Extension update not allowed.";
    if (!callback.is_null())
      std::move(callback).Run();
    return;
  }

  in_progress_updates_.push_back(
      InProgressUpdate(std::move(callback), update_params.install_immediately));
  InProgressUpdate& update = in_progress_updates_.back();

  // |update_data| only store update info of extensions that are not being
  // updated at the moment.
  ExtensionUpdateDataMap update_data;
  for (const auto& update_info : update_params.update_info) {
    const std::string& extension_id = update_info.first;

    DCHECK(!extension_id.empty());

    update.pending_extension_ids.insert(extension_id);
    if (updating_extension_ids_.count(extension_id) > 0)
      continue;

    updating_extension_ids_.insert(extension_id);

    ExtensionUpdateData data = update_info.second;
    if (data.is_corrupt_reinstall) {
      data.install_source = "reinstall";
    } else if (data.install_source.empty() &&
               update_params.priority ==
                   ExtensionUpdateCheckParams::FOREGROUND) {
      data.install_source = "ondemand";
    }
    update_data.insert(std::make_pair(extension_id, data));
  }

  // Divide extensions into batches to reduce the size of update check
  // requests generated by the update client.
  for (auto it = update_data.begin(); it != update_data.end();) {
    ExtensionUpdateDataMap batch_data;
    size_t batch_size =
        std::min(kMaxExtensionsPerUpdate,
                 static_cast<size_t>(std::distance(it, update_data.end())));

    std::vector<std::string> batch_ids;
    batch_ids.reserve(batch_size);
    for (size_t i = 0; i < batch_size; ++i, ++it) {
      batch_ids.push_back(it->first);
      batch_data.emplace(it->first, std::move(it->second));
    }

    update_client_->Update(
        batch_ids,
        base::BindOnce(&UpdateDataProvider::GetData, update_data_provider_,
                       update_params.install_immediately,
                       std::move(batch_data)),
        update_params.priority == ExtensionUpdateCheckParams::FOREGROUND,
        base::BindOnce(&UpdateService::UpdateCheckComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void UpdateService::UpdateCheckComplete(update_client::Error error) {
  VLOG(2) << "UpdateService::UpdateCheckComplete";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // There must be at least one in-progress update (the one that just
  // finished).
  DCHECK(!in_progress_updates_.empty());

  if (!in_progress_updates_[0].pending_extension_ids.empty()) {
    // This can happen when the update check request is batched.
    return;
  }

  // Find all updates that have finished and remove them from the list.
  in_progress_updates_.erase(
      std::remove_if(in_progress_updates_.begin(), in_progress_updates_.end(),
                     [](InProgressUpdate& update) {
                       if (!update.pending_extension_ids.empty())
                         return false;
                       VLOG(2) << "UpdateComplete";
                       if (!update.callback.is_null())
                         std::move(update.callback).Run();
                       return true;
                     }),
      in_progress_updates_.end());
}

void UpdateService::AddUpdateClientObserver(
    update_client::UpdateClient::Observer* observer) {
  if (update_client_)
    update_client_->AddObserver(observer);
}

void UpdateService::RemoveUpdateClientObserver(
    update_client::UpdateClient::Observer* observer) {
  if (update_client_)
    update_client_->RemoveObserver(observer);
}

void UpdateService::HandleComponentUpdateErrorEvent(
    const std::string& extension_id) const {
  update_client::ErrorCategory error_category =
      update_client::ErrorCategory::kNone;
  int error_code = 0;
  update_client::CrxUpdateItem update_item;
  if (update_client_->GetCrxUpdateState(extension_id, &update_item)) {
    error_category = update_item.error_category;
    error_code = update_item.error_code;
  }

  switch (error_category) {
    case update_client::ErrorCategory::kUpdateCheck:
      ReportUpdateCheckResult(ExtensionUpdaterUpdateResult::UPDATE_CHECK_ERROR,
                              error_code);
      break;
    case update_client::ErrorCategory::kDownload:
      ReportUpdateCheckResult(
          ExtensionUpdaterUpdateResult::UPDATE_DOWNLOAD_ERROR, error_code);
      break;
    case update_client::ErrorCategory::kUnpack:
    case update_client::ErrorCategory::kInstall:
      ReportUpdateCheckResult(
          ExtensionUpdaterUpdateResult::UPDATE_INSTALL_ERROR, 0);
      break;
    case update_client::ErrorCategory::kNone:
    case update_client::ErrorCategory::kService:
      ReportUpdateCheckResult(
          ExtensionUpdaterUpdateResult::UPDATE_SERVICE_ERROR, error_code);
      break;
  }
}

void UpdateService::HandleComponentUpdateFoundEvent(
    const std::string& extension_id) const {
  update_client::CrxUpdateItem update_item;
  if (!update_client_->GetCrxUpdateState(extension_id, &update_item)) {
    return;
  }

  VLOG(3) << "UpdateService::OnEvent COMPONENT_UPDATE_FOUND: " << extension_id
          << " " << update_item.next_version.GetString();
  UpdateDetails update_info(extension_id, update_item.next_version);
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND,
      content::NotificationService::AllBrowserContextsAndSources(),
      content::Details<UpdateDetails>(&update_info));
}

}  // namespace extensions
