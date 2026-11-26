// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_MODEL_BRING_ANDROID_TABS_TO_IOS_SERVICE_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_MODEL_BRING_ANDROID_TABS_TO_IOS_SERVICE_H_

#import <memory>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace bring_android_tabs {
enum class PromptAttemptStatus;
}  // namespace bring_android_tabs

namespace segmentation_platform {
class DeviceSwitcherResultDispatcher;
}  // namespace segmentation_platform

namespace syncer {
class SyncService;
}  // namespace syncer

namespace sync_sessions {
class SessionSyncService;
}  // namespace sync_sessions

namespace synced_sessions {
struct DistantTab;
class SyncedSessions;
}  // namespace synced_sessions

class UrlLoadingBrowserAgent;
enum class UrlLoadStrategy;

// Class that manages the life cycle of the "Bring tabs from Android"
// experience.
class BringAndroidTabsToIOSService : public KeyedService {
 public:
  explicit BringAndroidTabsToIOSService(
      segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher,
      syncer::SyncService* sync_service,
      sync_sessions::SessionSyncService* session_sync_service,
      PrefService* profile_prefs);

  BringAndroidTabsToIOSService(const BringAndroidTabsToIOSService&) = delete;
  BringAndroidTabsToIOSService& operator=(const BringAndroidTabsToIOSService&) =
      delete;

  ~BringAndroidTabsToIOSService() override;

  //  Loads a list of recent tabs brought from the user's last Android device
  //  into the service on demand. Could be called repeatedly to retrieve updated
  //  results.
  //
  //  Note that the recency of the session the tab is in takes precedence over
  //  the recency of the tab.
  void LoadTabs();

  // Returns the number of recent tabs / the tab at the given index.
  // `GetNumberOfAndroidTabs() > 0` could be used to determine whether the
  // prompt should be shown or not.
  //
  // NOTE: Both method MUST be called after `LoadTabs()` is called; the
  // returned values would be computed from the last call to `LoadTabs`.
  virtual size_t GetNumberOfAndroidTabs() const;
  virtual synced_sessions::DistantTab* GetTabAtIndex(size_t index) const;

  // Open the tabs in tab grid.
  virtual void OpenTabsAtIndices(const std::vector<size_t>& indices,
                                 UrlLoadingBrowserAgent* url_loader);
  virtual void OpenAllTabs(UrlLoadingBrowserAgent* url_loader);

  // Called when the Bring Android Tabs Prompt has been displayed.
  virtual void OnBringAndroidTabsPromptDisplayed();

  // Called when the user interacts with the Bring Android Tabs prompt.
  virtual void OnUserInteractWithBringAndroidTabsPrompt();

 private:
  // Returns true if the prompt has been shown before invocation, and should not
  // show again.
  bool PromptShownAndShouldNotShowAgain() const;
  // Loads the list of synced sessions and saves the positions of Android
  // Switcher Tabs and returns the result.
  bring_android_tabs::PromptAttemptStatus
  LoadSyncedSessionsAndComputeTabPositions();

  // Service dependencies.
  const raw_ptr<segmentation_platform::DeviceSwitcherResultDispatcher>
      device_switcher_result_dispatcher_;
  const raw_ptr<syncer::SyncService> sync_service_;
  raw_ptr<sync_sessions::SessionSyncService> session_sync_service_;
  const raw_ptr<PrefService> profile_prefs_;
  // Flag that marks whether `LoadTabs` has been invoked.
  bool load_tabs_invoked_ = false;

  // Prompt is displayed in the current browsing session.
  bool prompt_shown_current_session_ = false;
  // The user has interacted with the prompt in the current browsing session.
  bool prompt_interacted_ = false;
  // All synced sessions.
  std::unique_ptr<synced_sessions::SyncedSessions> synced_sessions_;
  // Positions of recent tabs from the user's last Android device in
  // `synced_sessions_`.
  std::vector<std::tuple<size_t, size_t>> position_of_tabs_in_synced_sessions_;
};

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_MODEL_BRING_ANDROID_TABS_TO_IOS_SERVICE_H_
