// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_browser_context.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/origin_trials/browser/leveldb_persistence_provider.h"
#include "components/origin_trials/browser/origin_trials.h"
#include "components/origin_trials/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_content_index_provider.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/shell/browser/shell_paths.h"
#include "content/shell/browser/shell_permission_manager.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/mock_background_sync_controller.h"
#include "content/test/mock_reduce_accept_language_controller_delegate.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace content {

ShellBrowserContext::ShellBrowserContext(bool off_the_record,
                                         bool delay_services_creation)
    : off_the_record_(off_the_record) {
  InitWhileIOAllowed();
#if BUILDFLAG(IS_WIN)
  base::SetExtraNoExecuteAllowedPath(SHELL_DIR_USER_DATA);
#endif  // BUILDFLAG(IS_WIN)

  if (!delay_services_creation) {
    BrowserContextDependencyManager::GetInstance()
        ->CreateBrowserContextServices(this);
  }
}

ShellBrowserContext::~ShellBrowserContext() {
  NotifyWillBeDestroyed();

  // The SimpleDependencyManager should always be passed after the
  // BrowserContextDependencyManager. This is because the KeyedService instances
  // in the BrowserContextDependencyManager's dependency graph can depend on the
  // ones in the SimpleDependencyManager's graph.
  DependencyManager::PerformInterlockedTwoPhaseShutdown(
      BrowserContextDependencyManager::GetInstance(), this,
      SimpleDependencyManager::GetInstance(), key_.get());

  SimpleKeyMap::GetInstance()->Dissociate(this);

  ShutdownStoragePartitions();
}

void ShellBrowserContext::InitWhileIOAllowed() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kIgnoreCertificateErrors))
    ignore_certificate_errors_ = true;

  if (cmd_line->HasSwitch(switches::kContentShellUserDataDir)) {
    path_ = cmd_line->GetSwitchValuePath(switches::kContentShellUserDataDir);
    if (base::DirectoryExists(path_) || base::CreateDirectory(path_))  {
      // BrowserContext needs an absolute path, which we would normally get via
      // PathService. In this case, manually ensure the path is absolute.
      if (!path_.IsAbsolute())
        path_ = base::MakeAbsoluteFilePath(path_);
      if (!path_.empty()) {
        FinishInitWhileIOAllowed();
        base::PathService::OverrideAndCreateIfNeeded(
            SHELL_DIR_USER_DATA, path_, /*is_absolute=*/true, /*create=*/false);
        return;
      }
    } else {
      LOG(WARNING) << "Unable to create data-path directory: " << path_.value();
    }
  }

  CHECK(base::PathService::Get(SHELL_DIR_USER_DATA, &path_));

  FinishInitWhileIOAllowed();
}

void ShellBrowserContext::FinishInitWhileIOAllowed() {
  key_ = std::make_unique<SimpleFactoryKey>(path_, off_the_record_);
  SimpleKeyMap::GetInstance()->Associate(this, key_.get());
}

std::unique_ptr<ZoomLevelDelegate> ShellBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath&) {
  return nullptr;
}

base::FilePath ShellBrowserContext::GetPath() {
  return path_;
}

bool ShellBrowserContext::IsOffTheRecord() {
  return off_the_record_;
}

DownloadManagerDelegate* ShellBrowserContext::GetDownloadManagerDelegate()  {
  if (!download_manager_delegate_.get()) {
    download_manager_delegate_ =
        std::make_unique<ShellDownloadManagerDelegate>();
    download_manager_delegate_->SetDownloadManager(GetDownloadManager());
  }

  return download_manager_delegate_.get();
}

BrowserPluginGuestManager* ShellBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* ShellBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

PlatformNotificationService*
ShellBrowserContext::GetPlatformNotificationService() {
  return nullptr;
}

PushMessagingService* ShellBrowserContext::GetPushMessagingService() {
  return nullptr;
}

StorageNotificationService*
ShellBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

SSLHostStateDelegate* ShellBrowserContext::GetSSLHostStateDelegate() {
  return nullptr;
}

PermissionControllerDelegate*
ShellBrowserContext::GetPermissionControllerDelegate() {
  if (!permission_manager_.get())
    permission_manager_ = std::make_unique<ShellPermissionManager>();
  return permission_manager_.get();
}

ClientHintsControllerDelegate*
ShellBrowserContext::GetClientHintsControllerDelegate() {
  return client_hints_controller_delegate_;
}

BackgroundFetchDelegate* ShellBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

BackgroundSyncController* ShellBrowserContext::GetBackgroundSyncController() {
  if (!background_sync_controller_) {
    background_sync_controller_ =
        std::make_unique<MockBackgroundSyncController>();
  }
  return background_sync_controller_.get();
}

BrowsingDataRemoverDelegate*
ShellBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

ContentIndexProvider* ShellBrowserContext::GetContentIndexProvider() {
  if (!content_index_provider_)
    content_index_provider_ = std::make_unique<ShellContentIndexProvider>();
  return content_index_provider_.get();
}

ReduceAcceptLanguageControllerDelegate*
ShellBrowserContext::GetReduceAcceptLanguageControllerDelegate() {
  if (!reduce_accept_lang_controller_delegate_) {
    reduce_accept_lang_controller_delegate_ =
        std::make_unique<MockReduceAcceptLanguageControllerDelegate>(
            GetShellLanguage());
  }
  return reduce_accept_lang_controller_delegate_.get();
}

OriginTrialsControllerDelegate*
ShellBrowserContext::GetOriginTrialsControllerDelegate() {
  if (!origin_trials::features::IsPersistentOriginTrialsEnabled())
    return nullptr;

  if (!origin_trials_controller_delegate_) {
    origin_trials_controller_delegate_ =
        std::make_unique<origin_trials::OriginTrials>(
            std::make_unique<origin_trials::LevelDbPersistenceProvider>(
                GetPath(),
                GetDefaultStoragePartition()->GetProtoDatabaseProvider()),
            std::make_unique<blink::TrialTokenValidator>());
  }
  return origin_trials_controller_delegate_.get();
}

}  // namespace content
