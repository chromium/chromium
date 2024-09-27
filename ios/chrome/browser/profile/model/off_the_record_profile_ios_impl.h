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

// The implementation of ProfileIOS that is used for incognito browsing.
// Each OffTheRecordProfileIOSImpl instance is associated with and owned
// by a non-incognito ProfileIOS instance.
class OffTheRecordProfileIOSImpl final : public ProfileIOS {
 public:
  OffTheRecordProfileIOSImpl(const OffTheRecordProfileIOSImpl&) = delete;
  OffTheRecordProfileIOSImpl& operator=(const OffTheRecordProfileIOSImpl&) =
      delete;

  ~OffTheRecordProfileIOSImpl() override;

  // ProfileIOS:
  // TODO(crbug.com/358299863): Remove these functions once fully migrated.
  ProfileIOS* GetOriginalChromeBrowserState() override;
  bool HasOffTheRecordChromeBrowserState() const override;
  ProfileIOS* GetOffTheRecordChromeBrowserState() override;
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
  const sync_preferences::PrefServiceSyncable* GetSyncablePrefs()
      const override;
  ProfileIOSIOData* GetIOData() override;
  void ClearNetworkingHistorySince(base::Time time,
                                   base::OnceClosure completion) override;
  net::URLRequestContextGetter* CreateRequestContext(
      ProtocolHandlerMap* protocol_handlers) override;
  base::WeakPtr<ProfileIOS> AsWeakPtr() override;

  // BrowserState:
  bool IsOffTheRecord() const override;

 private:
  friend class ProfileIOSImpl;

  // `original_profile` is the non-incognito ProfileIOS instance that
  // owns this instance.
  OffTheRecordProfileIOSImpl(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      ProfileIOS* original_profile,
      const base::FilePath& otr_path);

  raw_ptr<ProfileIOS> original_profile_;  // weak

  // Creation time of the off-the-record ProfileIOS.
  const base::Time start_time_;

  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;

  std::unique_ptr<OffTheRecordProfileIOSIOData::Handle> io_data_;
  std::unique_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  base::WeakPtrFactory<OffTheRecordProfileIOSImpl> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_PROFILE_MODEL_OFF_THE_RECORD_PROFILE_IOS_IMPL_H_
