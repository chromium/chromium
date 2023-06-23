// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_agent_util.h"

#import "base/feature_list.h"
#import "components/breadcrumbs/core/breadcrumbs_status.h"
#import "ios/chrome/browser/app_launcher/app_launcher_browser_agent.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_browser_agent.h"
#import "ios/chrome/browser/device_sharing/device_sharing_browser_agent.h"
#import "ios/chrome/browser/follow/follow_browser_agent.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/infobar_overlay_browser_agent_util.h"
#import "ios/chrome/browser/lens/lens_browser_agent.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/reading_list/reading_list_browser_agent.h"
#import "ios/chrome/browser/send_tab_to_self/send_tab_to_self_browser_agent.h"
#import "ios/chrome/browser/sessions/live_tab_context_browser_agent.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/sync/sync_error_browser_agent.h"
#import "ios/chrome/browser/tabs/closing_web_state_observer_browser_agent.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/tabs/synced_window_delegate_browser_agent.h"
#import "ios/chrome/browser/tabs/tab_parenting_browser_agent.h"
#import "ios/chrome/browser/tabs/tab_pickup/tab_pickup_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/upgrade/upgrade_center.h"
#import "ios/chrome/browser/upgrade/upgrade_center_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/web/page_placeholder_browser_agent.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/web_state_delegate_browser_agent.h"
#import "ios/chrome/browser/web/web_state_update_browser_agent.h"
#import "ios/chrome/browser/web_state_list/session_metrics.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/web_state_list/view_source_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list_metrics_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/public/provider/chrome/browser/app_utils/app_utils_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void AttachBrowserAgents(Browser* browser) {
  if (breadcrumbs::IsEnabled()) {
    BreadcrumbManagerBrowserAgent::CreateForBrowser(browser);
  }

  const bool browser_is_off_record =
      browser->GetBrowserState()->IsOffTheRecord();
  const bool browser_is_inactive = browser->IsInactive();

  LiveTabContextBrowserAgent::CreateForBrowser(browser);
  TabInsertionBrowserAgent::CreateForBrowser(browser);
  AttachInfobarOverlayBrowserAgent(browser);
  SyncedWindowDelegateBrowserAgent::CreateForBrowser(browser);
  WebUsageEnablerBrowserAgent::CreateForBrowser(browser);
  DeviceSharingBrowserAgent::CreateForBrowser(browser);
  UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser);
  AppLauncherBrowserAgent::CreateForBrowser(browser);

  // LensBrowserAgent must be created before WebNavigationBrowserAgent.
  LensBrowserAgent::CreateForBrowser(browser);
  WebNavigationBrowserAgent::CreateForBrowser(browser);
  TabParentingBrowserAgent::CreateForBrowser(browser);

  ClosingWebStateObserverBrowserAgent::CreateForBrowser(browser);
  SnapshotBrowserAgent::CreateForBrowser(browser);

  if (!browser_is_off_record && !browser_is_inactive) {
    TabPickupBrowserAgent::CreateForBrowser(browser);
  }

  if (IsWebChannelsEnabled() && !browser_is_off_record) {
    FollowBrowserAgent::CreateForBrowser(browser);
  }

  // PolicyWatcher is non-OTR only.
  if (!browser_is_off_record) {
    PolicyWatcherBrowserAgent::CreateForBrowser(browser);
  }

  // Send Tab To Self is non-OTR only.
  if (!browser_is_off_record) {
    SendTabToSelfBrowserAgent::CreateForBrowser(browser);
  }

  WebStateDelegateBrowserAgent::CreateForBrowser(
      browser, TabInsertionBrowserAgent::FromBrowser(browser));

  // ViewSourceBrowserAgent requires TabInsertionBrowserAgent, and is only used
  // in debug builds.
#if !defined(NDEBUG)
  ViewSourceBrowserAgent::CreateForBrowser(browser);
#endif  // !defined(NDEBUG)

  // UrlLoadingBrowserAgent requires UrlLoadingNotifierBrowserAgent.
  UrlLoadingBrowserAgent::CreateForBrowser(browser);

  // SessionRestorartionAgent requires WebUsageEnablerBrowserAgent.
  SessionRestorationBrowserAgent::CreateForBrowser(
      browser, [SessionServiceIOS sharedService], IsPinnedTabsEnabled());

  // TabUsageRecorderBrowserAgent and WebStateListMetricsBrowserAgent observe
  // the SessionRestorationBrowserAgent, so they should be created after the the
  // SessionRestorationBrowserAgent is created.
  WebStateListMetricsBrowserAgent::CreateForBrowser(
      browser, SessionMetrics::FromBrowserState(browser->GetBrowserState()));

  // Normal browser states are the only ones to get tab usage recorder.
  if (!browser_is_off_record) {
    TabUsageRecorderBrowserAgent::CreateForBrowser(browser);
  }

  if (!browser_is_off_record) {
    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser);
  }

  if (!browser_is_inactive) {
    SyncErrorBrowserAgent::CreateForBrowser(browser);
  }

  UpgradeCenterBrowserAgent::CreateForBrowser(browser,
                                              [UpgradeCenter sharedInstance]);
  WebStateUpdateBrowserAgent::CreateForBrowser(browser);
  ReadingListBrowserAgent::CreateForBrowser(browser);

  PagePlaceholderBrowserAgent::CreateForBrowser(browser);

  // This needs to be called last in case any downstream browser agents need to
  // access upstream agents created earlier in this function.
  ios::provider::AttachBrowserAgents(browser);
}
