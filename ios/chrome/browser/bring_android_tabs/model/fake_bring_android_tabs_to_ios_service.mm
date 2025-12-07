// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/model/fake_bring_android_tabs_to_ios_service.h"

#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions.h"

FakeBringAndroidTabsToIOSService::FakeBringAndroidTabsToIOSService(
    std::vector<std::unique_ptr<synced_sessions::DistantTab>> tabs,
    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher,
    syncer::SyncService* sync_service,
    sync_sessions::SessionSyncService* session_sync_service,
    PrefService* profile_prefs)
    : BringAndroidTabsToIOSService(dispatcher,
                                   sync_service,
                                   session_sync_service,
                                   profile_prefs),
      tabs_(std::move(tabs)) {}

FakeBringAndroidTabsToIOSService::~FakeBringAndroidTabsToIOSService() {}

size_t FakeBringAndroidTabsToIOSService::GetNumberOfAndroidTabs() const {
  return tabs_.size();
}

synced_sessions::DistantTab* FakeBringAndroidTabsToIOSService::GetTabAtIndex(
    size_t index) const {
  return tabs_[index].get();
}

void FakeBringAndroidTabsToIOSService::OpenTabsAtIndices(
    const std::vector<size_t>& indices,
    UrlLoadingBrowserAgent* url_loader) {
  opened_tabs_at_indices_ = indices;
}

void FakeBringAndroidTabsToIOSService::OnBringAndroidTabsPromptDisplayed() {
  displayed_ = true;
}

void FakeBringAndroidTabsToIOSService::
    OnUserInteractWithBringAndroidTabsPrompt() {
  interacted_ = true;
}

bool FakeBringAndroidTabsToIOSService::displayed() {
  return displayed_;
}

bool FakeBringAndroidTabsToIOSService::interacted() {
  return interacted_;
}

std::vector<size_t> FakeBringAndroidTabsToIOSService::opened_tabs_at_indices()
    const {
  return opened_tabs_at_indices_;
}
