// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/chrome_browser_state_impl.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/ios/proxy_service_factory.h"
#include "components/proxy_config/pref_proxy_config_tracker.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/bookmark_model_loaded_observer.h"
#include "ios/chrome/browser/browser_state/off_the_record_chrome_browser_state_impl.h"
#include "ios/chrome/browser/chrome_constants.h"
#include "ios/chrome/browser/chrome_paths_internal.h"
#include "ios/chrome/browser/file_metadata_util.h"
#include "ios/chrome/browser/net/ios_chrome_url_request_context_getter.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/prefs/ios_chrome_pref_service_factory.h"
#include "ios/chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "ios/web/public/thread/web_thread.h"

namespace {

// Returns a bool indicating whether the necessary directories were able to be
// created (or already existed).
bool EnsureBrowserStateDirectoriesCreated(const base::FilePath& path,
                                          const base::FilePath& otr_path,
                                          const base::FilePath& cache_path) {
  // Create the browser state directory synchronously otherwise we would need to
  // sequence every otherwise independent I/O operation inside the browser state
  // directory with this operation. base::CreateDirectory() should be a
  // lightweight I/O operation and avoiding the headache of sequencing all
  // otherwise unrelated I/O after this one justifies running it on the main
  // thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io_to_create_directory;

  if (!base::PathExists(path) && !base::CreateDirectory(path))
    return false;
  // Create the directory for the OTR stash state now, even though it won't
  // necessarily be needed: the OTR browser state itself is created
  // synchronously on an as-needed basis on the UI thread, so creation of its
  // stash state directory cannot easily be done at that point.
  if (!base::PathExists(otr_path) && !base::CreateDirectory(otr_path))
    return false;
  SetSkipSystemBackupAttributeToItem(otr_path, true);
  if (!base::PathExists(cache_path) && !base::CreateDirectory(cache_path))
    return false;
  return true;
}

base::FilePath GetCachePath(const base::FilePath& base) {
  return base.Append(kIOSChromeCacheDirname);
}

}  // namespace

ChromeBrowserStateImpl::ChromeBrowserStateImpl(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    const base::FilePath& path)
    : ChromeBrowserState(std::move(io_task_runner)),
      state_path_(path),
      pref_registry_(new user_prefs::PrefRegistrySyncable),
      io_data_(new ChromeBrowserStateImplIOData::Handle(this)) {
  otr_state_path_ = state_path_.Append(FILE_PATH_LITERAL("OTR"));

  // It would be nice to use PathService for fetching this directory, but
  // the cache directory depends on the browser state stash directory, which
  // isn't available to PathService.
  base::FilePath base_cache_path;
  ios::GetUserCacheDirectory(state_path_, &base_cache_path);

  bool directories_created = EnsureBrowserStateDirectoriesCreated(
      state_path_, otr_state_path_, base_cache_path);
  DCHECK(directories_created);

  RegisterBrowserStatePrefs(pref_registry_.get());
  BrowserStateDependencyManager::GetInstance()
      ->RegisterBrowserStatePrefsForServices(pref_registry_.get());

  prefs_ = CreateBrowserStatePrefs(state_path_, GetIOTaskRunner().get(),
                                   pref_registry_);
  // Register on BrowserState.
  user_prefs::UserPrefs::Set(this, prefs_.get());

  // Migrate obsolete prefs.
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  MigrateObsoleteLocalStatePrefs(local_state);
  MigrateObsoleteBrowserStatePrefs(prefs_.get());

  BrowserStateDependencyManager::GetInstance()->CreateBrowserStateServices(
      this);

  base::FilePath cookie_path = state_path_.Append(kIOSChromeCookieFilename);
  base::FilePath cache_path = GetCachePath(base_cache_path);
  int cache_max_size = 0;

  // Make sure we initialize the io_data_ after everything else has been
  // initialized that we might be reading from the IO thread.
  io_data_->Init(cookie_path, cache_path, cache_max_size, state_path_);

  // Listen for bookmark model load, to bootstrap the sync service.
  bookmarks::BookmarkModel* model =
      ios::BookmarkModelFactory::GetForBrowserState(this);
  model->AddObserver(new BookmarkModelLoadedObserver(this));

  send_tab_to_self::SendTabToSelfClientServiceFactory::GetForBrowserState(this);
}

ChromeBrowserStateImpl::~ChromeBrowserStateImpl() {
  BrowserStateDependencyManager::GetInstance()->DestroyBrowserStateServices(
      this);
  if (pref_proxy_config_tracker_)
    pref_proxy_config_tracker_->DetachFromPrefService();
  DestroyOffTheRecordChromeBrowserState();
}

ios::ChromeBrowserState*
ChromeBrowserStateImpl::GetOriginalChromeBrowserState() {
  return this;
}

ios::ChromeBrowserState*
ChromeBrowserStateImpl::GetOffTheRecordChromeBrowserState() {
  if (!otr_state_) {
    otr_state_.reset(new OffTheRecordChromeBrowserStateImpl(
        GetIOTaskRunner(), this, otr_state_path_));
  }

  return otr_state_.get();
}

bool ChromeBrowserStateImpl::HasOffTheRecordChromeBrowserState() const {
  return !!otr_state_;
}

void ChromeBrowserStateImpl::DestroyOffTheRecordChromeBrowserState() {
  // Tear down both the OTR ChromeBrowserState and the OTR Profile with which
  // it is associated.
  otr_state_.reset();
}

PrefService* ChromeBrowserStateImpl::GetPrefs() {
  DCHECK(prefs_);  // Should explicitly be initialized.
  return prefs_.get();
}

bool ChromeBrowserStateImpl::IsOffTheRecord() const {
  return false;
}

base::FilePath ChromeBrowserStateImpl::GetStatePath() const {
  return state_path_;
}

void ChromeBrowserStateImpl::SetOffTheRecordChromeBrowserState(
    std::unique_ptr<ios::ChromeBrowserState> otr_state) {
  DCHECK(!otr_state_);
  otr_state_ = std::move(otr_state);
}

PrefService* ChromeBrowserStateImpl::GetOffTheRecordPrefs() {
  DCHECK(prefs_);
  if (!otr_prefs_) {
    otr_prefs_ = CreateIncognitoBrowserStatePrefs(prefs_.get());
  }
  return otr_prefs_.get();
}

ChromeBrowserStateIOData* ChromeBrowserStateImpl::GetIOData() {
  return io_data_->io_data();
}

net::URLRequestContextGetter* ChromeBrowserStateImpl::CreateRequestContext(
    ProtocolHandlerMap* protocol_handlers) {
  ApplicationContext* application_context = GetApplicationContext();
  return io_data_
      ->CreateMainRequestContextGetter(
          protocol_handlers, application_context->GetLocalState(),
          application_context->GetIOSChromeIOThread())
      .get();
}

void ChromeBrowserStateImpl::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
  io_data_->ClearNetworkingHistorySince(time, completion);
}

PrefProxyConfigTracker* ChromeBrowserStateImpl::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_) {
    pref_proxy_config_tracker_ =
        ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(
            GetPrefs(), GetApplicationContext()->GetLocalState());
  }
  return pref_proxy_config_tracker_.get();
}
