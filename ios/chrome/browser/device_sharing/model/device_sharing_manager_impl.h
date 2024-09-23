// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_MANAGER_IMPL_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "base/gtest_prod_util.h"
#import "base/memory/raw_ptr.h"
#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class Browser;
@class HandoffManager;

class DeviceSharingManagerImpl : public DeviceSharingManager {
 public:
  explicit DeviceSharingManagerImpl(ProfileIOS* profile);

  // Not copyable or moveable.
  DeviceSharingManagerImpl(const DeviceSharingManagerImpl&) = delete;
  DeviceSharingManagerImpl& operator=(const DeviceSharingManagerImpl&) = delete;
  ~DeviceSharingManagerImpl() override;

  void SetActiveBrowser(Browser* browser) override;
  void UpdateActiveUrl(Browser* browser, const GURL& active_url) override;
  void UpdateActiveTitle(Browser* browser,
                         const std::u16string& title) override;
  void ClearActiveUrl(Browser* browser) override;

 private:
  // Allow tests to inspect the handoff manager.
  friend class DeviceSharingManagerImplTest;
  friend class DeviceSharingBrowserAgentTest;
  friend NSURL* GetCurrentUserActivityURL(ProfileIOS* profile);

  void UpdateHandoffManager();

  raw_ptr<ProfileIOS> profile_ = nullptr;

  // Registrar for pref change notifications to the active profile.
  PrefChangeRegistrar prefs_change_observer_;

  // Responsible for maintaining all state related to the Handoff feature.
  __strong HandoffManager* handoff_manager_;

  // The current active browser.
  raw_ptr<Browser> active_browser_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_MANAGER_IMPL_H_
