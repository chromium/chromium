// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_MODEL_FAKE_BRING_ANDROID_TABS_TO_IOS_SERVICE_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_MODEL_FAKE_BRING_ANDROID_TABS_TO_IOS_SERVICE_H_

#import "ios/chrome/browser/bring_android_tabs/model/bring_android_tabs_to_ios_service.h"

// A fake BringAndroidTabsToIOSService for testing purpose. Takes a set of tabs
// as input.
class FakeBringAndroidTabsToIOSService : public BringAndroidTabsToIOSService {
 public:
  FakeBringAndroidTabsToIOSService(
      std::vector<std::unique_ptr<synced_sessions::DistantTab>> tabs,
      segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher,
      syncer::SyncService* sync_service,
      sync_sessions::SessionSyncService* session_sync_service,
      PrefService* profile_prefs);

  ~FakeBringAndroidTabsToIOSService() override;

  // BringAndroidTabsToIOSService implementation.
  size_t GetNumberOfAndroidTabs() const override;
  synced_sessions::DistantTab* GetTabAtIndex(size_t index) const override;
  void OpenTabsAtIndices(const std::vector<size_t>& indices,
                         UrlLoadingBrowserAgent* url_loader) override;
  void OnBringAndroidTabsPromptDisplayed() override;
  void OnUserInteractWithBringAndroidTabsPrompt() override;

  // Returns true if OnBringAndroidTabsPromptDisplayed() was called.
  bool displayed();
  // Returns true if OnUserInteractWithBringAndroidTabsPrompt() was called.
  bool interacted();
  // Returns the indices of the tabs opened.
  std::vector<size_t> opened_tabs_at_indices() const;

 private:
  std::vector<std::unique_ptr<synced_sessions::DistantTab>> tabs_;
  bool displayed_ = false;
  bool interacted_ = false;
  std::vector<size_t> opened_tabs_at_indices_;
};

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_MODEL_FAKE_BRING_ANDROID_TABS_TO_IOS_SERVICE_H_
