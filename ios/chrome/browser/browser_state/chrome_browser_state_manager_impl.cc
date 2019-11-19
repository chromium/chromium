// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/chrome_browser_state_manager_impl.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "components/signin/ios/browser/active_state_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_impl.h"
#include "ios/chrome/browser/browser_state/off_the_record_chrome_browser_state_impl.h"
#include "ios/chrome/browser/browser_state_metrics/browser_state_metrics.h"
#include "ios/chrome/browser/chrome_constants.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/signin/account_consistency_service_factory.h"
#include "ios/chrome/browser/signin/account_reconcilor_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"

namespace {

int64_t ComputeFilesSize(const base::FilePath& directory,
                         const base::FilePath::StringType& pattern) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int64_t running_size = 0;
  base::FileEnumerator iter(directory, false, base::FileEnumerator::FILES,
                            pattern);
  while (!iter.Next().empty())
    running_size += iter.GetInfo().GetSize();
  return running_size;
}

// Simple task to log the size of the browser state at |path|.
void BrowserStateSizeTask(const base::FilePath& path) {
  const int64_t kBytesInOneMB = 1024 * 1024;

  int64_t size = ComputeFilesSize(path, FILE_PATH_LITERAL("*"));
  int size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.HistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalHistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Cookies"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.CookiesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Bookmarks"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.BookmarksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Favicons"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.FaviconsSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Top Sites"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TopSitesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Visited Links"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.VisitedLinksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Web Data"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.WebDataSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Extension*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.ExtensionSize", size_MB);
}

// Gets the user data directory.
base::FilePath GetUserDataDir() {
  base::FilePath user_data_dir;
  bool result = base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir);
  DCHECK(result);
  return user_data_dir;
}

}  // namespace

ChromeBrowserStateManagerImpl::ChromeBrowserStateManagerImpl() {}

ChromeBrowserStateManagerImpl::~ChromeBrowserStateManagerImpl() {
  for (const auto& pair : browser_states_) {
    ChromeBrowserStateImpl* browser_state = pair.second.get();
    ActiveStateManager::FromBrowserState(browser_state)->SetActive(false);
    if (!browser_state->HasOffTheRecordChromeBrowserState())
      continue;

    web::BrowserState* otr_browser_state =
        browser_state->GetOffTheRecordChromeBrowserState();
    if (!ActiveStateManager::ExistsForBrowserState(otr_browser_state))
      continue;
    ActiveStateManager::FromBrowserState(otr_browser_state)->SetActive(false);
  }
}

ios::ChromeBrowserState*
ChromeBrowserStateManagerImpl::GetLastUsedBrowserState() {
  return GetBrowserState(GetLastUsedBrowserStateDir(GetUserDataDir()));
}

ios::ChromeBrowserState* ChromeBrowserStateManagerImpl::GetBrowserState(
    const base::FilePath& path) {
  // If the browser state is already loaded, just return it.
  auto iter = browser_states_.find(path);
  if (iter != browser_states_.end()) {
    DCHECK(iter->second.get());
    return iter->second.get();
  }

  // Get sequenced task runner for making sure that file operations of
  // this profile are executed in expected order (what was previously assured by
  // the FILE thread).
  scoped_refptr<base::SequencedTaskRunner> io_task_runner =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
           base::MayBlock()});

  std::unique_ptr<ChromeBrowserStateImpl> browser_state_impl(
      new ChromeBrowserStateImpl(io_task_runner, path));
  DCHECK(!browser_state_impl->IsOffTheRecord());

  std::pair<ChromeBrowserStateImplPathMap::iterator, bool> insert_result =
      browser_states_.insert(
          std::make_pair(path, std::move(browser_state_impl)));
  DCHECK(insert_result.second);
  DCHECK(insert_result.first != browser_states_.end());

  DoFinalInit(insert_result.first->second.get());
  return insert_result.first->second.get();
}

base::FilePath ChromeBrowserStateManagerImpl::GetLastUsedBrowserStateDir(
    const base::FilePath& user_data_dir) {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  DCHECK(local_state);
  std::string last_used_browser_state_name =
      local_state->GetString(prefs::kBrowserStateLastUsed);
  if (last_used_browser_state_name.empty())
    last_used_browser_state_name = kIOSChromeInitialBrowserState;
  return user_data_dir.AppendASCII(last_used_browser_state_name);
}

BrowserStateInfoCache*
ChromeBrowserStateManagerImpl::GetBrowserStateInfoCache() {
  if (!browser_state_info_cache_) {
    browser_state_info_cache_.reset(new BrowserStateInfoCache(
        GetApplicationContext()->GetLocalState(), GetUserDataDir()));
  }
  return browser_state_info_cache_.get();
}

std::vector<ios::ChromeBrowserState*>
ChromeBrowserStateManagerImpl::GetLoadedBrowserStates() {
  std::vector<ios::ChromeBrowserState*> loaded_browser_states;
  for (const auto& pair : browser_states_)
    loaded_browser_states.push_back(pair.second.get());
  return loaded_browser_states;
}

void ChromeBrowserStateManagerImpl::DoFinalInit(
    ios::ChromeBrowserState* browser_state) {
  DoFinalInitForServices(browser_state);
  AddBrowserStateToCache(browser_state);

  // Log the browser state size after a reasonable startup delay.
  base::FilePath path =
      browser_state->GetOriginalChromeBrowserState()->GetStatePath();
  base::PostDelayedTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&BrowserStateSizeTask, path),
      base::TimeDelta::FromSeconds(112));

  LogNumberOfBrowserStates(
      GetApplicationContext()->GetChromeBrowserStateManager());
}

void ChromeBrowserStateManagerImpl::DoFinalInitForServices(
    ios::ChromeBrowserState* browser_state) {
  ios::AccountConsistencyServiceFactory::GetForBrowserState(browser_state);
  IdentityManagerFactory::GetForBrowserState(browser_state)
      ->OnNetworkInitialized();
  ios::AccountReconcilorFactory::GetForBrowserState(browser_state);
  // Initialization needs to happen after the browser context is available
  // because UnifiedConsentService's dependencies needs the URL context getter.
  UnifiedConsentServiceFactory::GetForBrowserState(browser_state);
}

void ChromeBrowserStateManagerImpl::AddBrowserStateToCache(
    ios::ChromeBrowserState* browser_state) {
  DCHECK(!browser_state->IsOffTheRecord());
  BrowserStateInfoCache* cache = GetBrowserStateInfoCache();
  if (browser_state->GetStatePath().DirName() != cache->GetUserDataDir())
    return;

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  CoreAccountInfo account_info = identity_manager->GetPrimaryAccountInfo();
  base::string16 username = base::UTF8ToUTF16(account_info.email);

  size_t browser_state_index =
      cache->GetIndexOfBrowserStateWithPath(browser_state->GetStatePath());
  if (browser_state_index != std::string::npos) {
    // The BrowserStateInfoCache's info must match the IdentityManager.
    cache->SetAuthInfoOfBrowserStateAtIndex(browser_state_index,
                                            account_info.gaia, username);
    return;
  }
  cache->AddBrowserState(browser_state->GetStatePath(), account_info.gaia,
                         username);
}
