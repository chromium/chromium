// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/model/browser_agent_util.h"

#import "base/feature_list.h"
#import "components/breadcrumbs/core/breadcrumbs_status.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_browser_agent.h"
#import "ios/chrome/browser/crash_report/model/breadcrumbs/breadcrumb_manager_browser_agent.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_browser_agent.h"
#import "ios/chrome/browser/favicon/model/favicon_browser_agent.h"
#import "ios/chrome/browser/follow/model/follow_browser_agent.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/infobar_overlay_browser_agent_util.h"
#import "ios/chrome/browser/intents/user_activity_browser_agent.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/tab_based_iph_browser_agent.h"
#import "ios/chrome/browser/lens/model/lens_browser_agent.h"
#import "ios/chrome/browser/metrics/model/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/metrics/model/web_state_list_metrics_browser_agent.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"
#import "ios/chrome/browser/sessions/model/live_tab_context_browser_agent.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/sync/model/sync_error_browser_agent.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/tabs/model/closing_web_state_observer_browser_agent.h"
#import "ios/chrome/browser/tabs/model/synced_window_delegate_browser_agent.h"
#import "ios/chrome/browser/tabs/model/tab_parenting_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/view_source/model/view_source_browser_agent.h"
#import "ios/chrome/browser/web/model/page_placeholder_browser_agent.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web/model/web_state_delegate_browser_agent.h"
#import "ios/chrome/browser/web/model/web_state_update_browser_agent.h"
#import "ios/chrome/browser/web_state_list/model/session_metrics.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/public/provider/chrome/browser/app_utils/app_utils_api.h"

void AttachBrowserAgents(Browser* browser) {
  if (breadcrumbs::IsEnabled(GetApplicationContext()->GetLocalState())) {
    BreadcrumbManagerBrowserAgent::CreateForBrowser(browser);
  }

  const bool browser_is_off_record = browser->GetProfile()->IsOffTheRecord();
  const bool browser_is_inactive = browser->IsInactive();

  LiveTabContextBrowserAgent::CreateForBrowser(browser);
  TabInsertionBrowserAgent::CreateForBrowser(browser);
  AttachInfobarOverlayBrowserAgent(browser);
  SyncedWindowDelegateBrowserAgent::CreateForBrowser(browser);
  WebUsageEnablerBrowserAgent::CreateForBrowser(browser);
  DeviceSharingBrowserAgent::CreateForBrowser(browser);
  UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser);
  AppLauncherBrowserAgent::CreateForBrowser(browser);
  OmniboxPositionBrowserAgent::CreateForBrowser(browser);

  // LensBrowserAgent must be created before WebNavigationBrowserAgent.
  LensBrowserAgent::CreateForBrowser(browser);
  WebNavigationBrowserAgent::CreateForBrowser(browser);
  TabParentingBrowserAgent::CreateForBrowser(browser);

  if (!browser_is_off_record) {
    ClosingWebStateObserverBrowserAgent::CreateForBrowser(browser);
  }

  SnapshotBrowserAgent::CreateForBrowser(browser);

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

  // TabUsageRecorderBrowserAgent and WebStateListMetricsBrowserAgent observe
  // the SessionRestorationBrowserAgent, so they should be created after the the
  // SessionRestorationBrowserAgent is created.
  WebStateListMetricsBrowserAgent::CreateForBrowser(
      browser, SessionMetrics::FromProfile(browser->GetProfile()));

  // Normal profiles are the only ones to get tab usage recorder.
  if (!browser_is_off_record) {
    TabUsageRecorderBrowserAgent::CreateForBrowser(browser);
  }

  if (!browser_is_off_record) {
    StartSurfaceRecentTabBrowserAgent::CreateForBrowser(browser);
  }

  if (!browser_is_inactive) {
    SyncErrorBrowserAgent::CreateForBrowser(browser);
  }

  WebStateUpdateBrowserAgent::CreateForBrowser(browser);
  ReadingListBrowserAgent::CreateForBrowser(browser);

  PagePlaceholderBrowserAgent::CreateForBrowser(browser);
  FaviconBrowserAgent::CreateForBrowser(browser);

  UserActivityBrowserAgent::CreateForBrowser(browser);

  if (!browser_is_inactive) {
    TabBasedIPHBrowserAgent::CreateForBrowser(browser);
  }

  // This needs to be called last in case any downstream browser agents need to
  // access upstream agents created earlier in this function.
  ios::provider::AttachBrowserAgents(browser);
}
