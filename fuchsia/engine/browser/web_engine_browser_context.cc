// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_browser_context.h"

#include <memory>
#include <utility>

#include "base/base_paths_fuchsia.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/strings/grit/components_locale_settings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "fuchsia/engine/browser/web_engine_net_log_observer.h"
#include "fuchsia/engine/browser/web_engine_permission_delegate.h"
#include "fuchsia/engine/switches.h"
#include "media/capabilities/in_memory_video_decode_stats_db_impl.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "services/network/public/cpp/network_switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Determines whether a data directory is configured, and returns its path.
// Passes the quota, if specified, for SysInfo to report as total disk space.
base::FilePath InitializeDataDirectoryAndQuotaFromCommandLine() {
  base::FilePath data_directory_path;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!base::PathService::Get(base::DIR_APP_DATA, &data_directory_path) ||
      !base::PathExists(data_directory_path)) {
    // Run in incognito mode if /data doesn't exist.
    return base::FilePath();
  }

  if (command_line->HasSwitch(switches::kDataQuotaBytes)) {
    // Configure SysInfo to use the specified quota as the total-disk-space
    // for the |data_dir_path_|.
    uint64_t quota_bytes = 0;
    CHECK(base::StringToUint64(
        command_line->GetSwitchValueASCII(switches::kDataQuotaBytes),
        &quota_bytes));
    base::SysInfo::SetAmountOfTotalDiskSpace(data_directory_path, quota_bytes);
  }

  return data_directory_path;
}

std::unique_ptr<WebEngineNetLogObserver> CreateNetLogObserver() {
  std::unique_ptr<WebEngineNetLogObserver> result;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(network::switches::kLogNetLog)) {
    base::FilePath log_path =
        command_line->GetSwitchValuePath(network::switches::kLogNetLog);
    result = std::make_unique<WebEngineNetLogObserver>(log_path);
  }

  return result;
}

}  // namespace

class WebEngineBrowserContext::ResourceContext
    : public content::ResourceContext {
 public:
  ResourceContext() = default;
  ~ResourceContext() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceContext);
};

WebEngineBrowserContext::WebEngineBrowserContext(bool force_incognito)
    : net_log_observer_(CreateNetLogObserver()),
      resource_context_(new ResourceContext()) {
  if (!force_incognito) {
    data_dir_path_ = InitializeDataDirectoryAndQuotaFromCommandLine();
  }

  simple_factory_key_ =
      std::make_unique<SimpleFactoryKey>(GetPath(), IsOffTheRecord());
  SimpleKeyMap::GetInstance()->Associate(this, simple_factory_key_.get());

  // TODO(crbug.com/1181156): Should apply any persisted isolated origins here.
  // However, since WebEngine does not persist any, that would currently be a
  // no-op.
}

WebEngineBrowserContext::~WebEngineBrowserContext() {
  SimpleKeyMap::GetInstance()->Dissociate(this);
  NotifyWillBeDestroyed(this);

  if (resource_context_) {
    content::GetIOThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, std::move(resource_context_));
  }

  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);

  ShutdownStoragePartitions();
}

std::unique_ptr<content::ZoomLevelDelegate>
WebEngineBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return nullptr;
}

base::FilePath WebEngineBrowserContext::GetPath() {
  return data_dir_path_;
}

bool WebEngineBrowserContext::IsOffTheRecord() {
  return data_dir_path_.empty();
}

content::ResourceContext* WebEngineBrowserContext::GetResourceContext() {
  return resource_context_.get();
}

content::DownloadManagerDelegate*
WebEngineBrowserContext::GetDownloadManagerDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

content::BrowserPluginGuestManager* WebEngineBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy*
WebEngineBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PushMessagingService*
WebEngineBrowserContext::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
WebEngineBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate*
WebEngineBrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

content::PermissionControllerDelegate*
WebEngineBrowserContext::GetPermissionControllerDelegate() {
  if (!permission_delegate_)
    permission_delegate_ = std::make_unique<WebEnginePermissionDelegate>();
  return permission_delegate_.get();
}

content::ClientHintsControllerDelegate*
WebEngineBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate*
WebEngineBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
WebEngineBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
WebEngineBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

media::VideoDecodePerfHistory*
WebEngineBrowserContext::GetVideoDecodePerfHistory() {
  if (IsOffTheRecord())
    return GetInMemoryVideoDecodePerfHistory();

  // Delegate to the base class for stateful VideoDecodePerfHistory DB
  // creation.
  return BrowserContext::GetVideoDecodePerfHistory();
}

std::string WebEngineBrowserContext::GetPreferredLanguages() const {
  return l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES);
}

media::VideoDecodePerfHistory*
WebEngineBrowserContext::GetInMemoryVideoDecodePerfHistory() {
  constexpr char kUserDataKeyName[] = "video-decode-perf-history";
  auto* decode_history = static_cast<media::VideoDecodePerfHistory*>(
      GetUserData(kUserDataKeyName));

  // Get, and potentially lazily create, the in-memory VideoDecodePerfHistory
  // DB.
  if (!decode_history) {
    auto owned_decode_history = std::make_unique<media::VideoDecodePerfHistory>(
        std::make_unique<media::InMemoryVideoDecodeStatsDBImpl>(
            nullptr /* seed_db_provider */),
        media::learning::FeatureProviderFactoryCB());
    decode_history = owned_decode_history.get();
    SetUserData(kUserDataKeyName, std::move(owned_decode_history));
  }

  return decode_history;
}
