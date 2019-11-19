// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/off_the_record_chrome_browser_state_impl.h"

#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/proxy_config/ios/proxy_service_factory.h"
#include "components/proxy_config/pref_proxy_config_tracker.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/net/ios_chrome_url_request_context_getter.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

OffTheRecordChromeBrowserStateImpl::OffTheRecordChromeBrowserStateImpl(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    ios::ChromeBrowserState* original_chrome_browser_state,
    const base::FilePath& otr_path)
    : ChromeBrowserState(std::move(io_task_runner)),
      otr_state_path_(otr_path),
      original_chrome_browser_state_(original_chrome_browser_state),
      prefs_(static_cast<sync_preferences::PrefServiceSyncable*>(
          original_chrome_browser_state->GetOffTheRecordPrefs())) {
  user_prefs::UserPrefs::Set(this, GetPrefs());
  io_data_.reset(new OffTheRecordChromeBrowserStateIOData::Handle(this));
  BrowserStateDependencyManager::GetInstance()->CreateBrowserStateServices(
      this);
}

OffTheRecordChromeBrowserStateImpl::~OffTheRecordChromeBrowserStateImpl() {
  BrowserStateDependencyManager::GetInstance()->DestroyBrowserStateServices(
      this);
  if (pref_proxy_config_tracker_)
    pref_proxy_config_tracker_->DetachFromPrefService();

  // Clears any data the network stack contains that may be related to the
  // OTR session.
  GetApplicationContext()->GetIOSChromeIOThread()->ChangedToOnTheRecord();
}

ios::ChromeBrowserState*
OffTheRecordChromeBrowserStateImpl::GetOriginalChromeBrowserState() {
  return original_chrome_browser_state_;
}

bool OffTheRecordChromeBrowserStateImpl::HasOffTheRecordChromeBrowserState()
    const {
  return true;
}

ios::ChromeBrowserState*
OffTheRecordChromeBrowserStateImpl::GetOffTheRecordChromeBrowserState() {
  return this;
}

void OffTheRecordChromeBrowserStateImpl::
    DestroyOffTheRecordChromeBrowserState() {
  NOTREACHED();
}

PrefService* OffTheRecordChromeBrowserStateImpl::GetPrefs() {
  return prefs_;
}

PrefService* OffTheRecordChromeBrowserStateImpl::GetOffTheRecordPrefs() {
  return GetPrefs();
}

bool OffTheRecordChromeBrowserStateImpl::IsOffTheRecord() const {
  return true;
}

base::FilePath OffTheRecordChromeBrowserStateImpl::GetStatePath() const {
  return otr_state_path_;
}

PrefProxyConfigTracker*
OffTheRecordChromeBrowserStateImpl::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_) {
    pref_proxy_config_tracker_ =
        ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(
            GetPrefs(), GetApplicationContext()->GetLocalState());
  }
  return pref_proxy_config_tracker_.get();
}

ChromeBrowserStateIOData* OffTheRecordChromeBrowserStateImpl::GetIOData() {
  return io_data_->io_data();
}

net::URLRequestContextGetter*
OffTheRecordChromeBrowserStateImpl::CreateRequestContext(
    ProtocolHandlerMap* protocol_handlers) {
  return io_data_->CreateMainRequestContextGetter(protocol_handlers).get();
}

void OffTheRecordChromeBrowserStateImpl::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
  // Nothing to do here, our transport security state is read-only.
  // Still, fire the callback to indicate we have finished, otherwise the
  // BrowsingDataRemover will never be destroyed and the dialog will never be
  // closed. We must do this asynchronously in order to avoid reentrancy issues.
  if (!completion.is_null()) {
    base::PostTask(FROM_HERE, {web::WebThread::UI}, completion);
  }
}
