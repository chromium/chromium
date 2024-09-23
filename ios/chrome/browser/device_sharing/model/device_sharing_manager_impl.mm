// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_sharing/model/device_sharing_manager_impl.h"

#import "components/handoff/handoff_manager.h"
#import "components/handoff/pref_names_ios.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

DeviceSharingManagerImpl::DeviceSharingManagerImpl(ProfileIOS* profile)
    : profile_(profile) {
  DCHECK(!profile || !profile->IsOffTheRecord());
  prefs_change_observer_.Init(profile_->GetPrefs());
  prefs_change_observer_.Add(
      prefs::kIosHandoffToOtherDevices,
      base::BindRepeating(&DeviceSharingManagerImpl::UpdateHandoffManager,
                          base::Unretained(this)));
  UpdateHandoffManager();
  [handoff_manager_ updateActiveURL:GURL()];
  [handoff_manager_ updateActiveTitle:std::u16string()];
}

DeviceSharingManagerImpl::~DeviceSharingManagerImpl() = default;

void DeviceSharingManagerImpl::SetActiveBrowser(Browser* browser) {
  active_browser_ = browser;
}

void DeviceSharingManagerImpl::UpdateActiveUrl(Browser* browser,
                                               const GURL& active_url) {
  if (browser != active_browser_)
    return;

  if (active_url.is_empty()) {
    ClearActiveUrl(browser);
    return;
  }

  [handoff_manager_ updateActiveURL:active_url];
}

void DeviceSharingManagerImpl::UpdateActiveTitle(Browser* browser,
                                                 const std::u16string& title) {
  if (browser != active_browser_)
    return;

  [handoff_manager_ updateActiveTitle:title];
}

void DeviceSharingManagerImpl::ClearActiveUrl(Browser* browser) {
  if (browser != active_browser_)
    return;

  [handoff_manager_ updateActiveURL:GURL()];
  [handoff_manager_ updateActiveTitle:std::u16string()];
}

void DeviceSharingManagerImpl::UpdateHandoffManager() {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kIosHandoffToOtherDevices)) {
    handoff_manager_ = nil;
    return;
  }

  if (!handoff_manager_)
    handoff_manager_ = [[HandoffManager alloc] init];
}
