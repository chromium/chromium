// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROFILE_MODEL_OFF_THE_RECORD_PROFILE_IOS_IMPL_H_
#define IOS_CHROME_BROWSER_PROFILE_MODEL_OFF_THE_RECORD_PROFILE_IOS_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "ios/chrome/browser/profile/model/off_the_record_profile_ios_io_data.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace policy {
class UserCloudPolicyManager;
}

// The implementation of ChromeBrowserState that is used for incognito browsing.
// Each OffTheRecordChromeBrowserStateImpl instance is associated with and owned
// by a non-incognito ChromeBrowserState instance.
class OffTheRecordChromeBrowserStateImpl final : public ChromeBrowserState {
 public:
  OffTheRecordChromeBrowserStateImpl(
      const OffTheRecordChromeBrowserStateImpl&) = delete;
  OffTheRecordChromeBrowserStateImpl& operator=(
      const OffTheRecordChromeBrowserStateImpl&) = delete;

  ~OffTheRecordChromeBrowserStateImpl() override;

  // ChromeBrowserState:
  // TODO(crbug.com/358299863): Remove these functions once fully migrated.
  ChromeBrowserState* GetOriginalChromeBrowserState() override;
  bool HasOffTheRecordChromeBrowserState() const override;
  ChromeBrowserState* GetOffTheRecordChromeBrowserState() override;
  void DestroyOffTheRecordChromeBrowserState() override;

  // ProfileIOS:
  ProfileIOS* GetOriginalProfile() override;
  bool HasOffTheRecordProfile() const override;
  ProfileIOS* GetOffTheRecordProfile() override;
  void DestroyOffTheRecordProfile() override;
  PrefProxyConfigTracker* GetProxyConfigTracker() override;
  BrowserStatePolicyConnector* GetPolicyConnector() override;
  policy::UserCloudPolicyManager* GetUserCloudPolicyManager() override;
  sync_preferences::PrefServiceSyncable* GetSyncablePrefs() override;
  ProfileIOSIOData* GetIOData() override;
  void ClearNetworkingHistorySince(base::Time time,
                                   base::OnceClosure completion) override;
  net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers) override;
  base::WeakPtr<ChromeBrowserState> AsWeakPtr() override;

  // BrowserState:
  bool IsOffTheRecord() const override;

 private:
  friend class ChromeBrowserStateImpl;

  // `original_chrome_browser_state_` is the non-incognito
  // ChromeBrowserState instance that owns this instance.
  OffTheRecordChromeBrowserStateImpl(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      ChromeBrowserState* original_chrome_browser_state,
      const base::FilePath& otr_path);

  raw_ptr<ChromeBrowserState> original_chrome_browser_state_;  // weak

  // Creation time of the off-the-record BrowserState.
  const base::Time start_time_;

  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;

  std::unique_ptr<OffTheRecordProfileIOSIOData::Handle> io_data_;
  std::unique_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  base::WeakPtrFactory<OffTheRecordChromeBrowserStateImpl> weak_ptr_factory_{
      this};
};

using OffTheRecordProfileIOSImpl = OffTheRecordChromeBrowserStateImpl;

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_OFF_THE_RECORD_PROFILE_IOS_IMPL_H_
