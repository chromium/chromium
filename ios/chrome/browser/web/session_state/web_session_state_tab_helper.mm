// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/session_state/web_session_state_tab_helper.h"

#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/path_service.h"
#import "base/strings/string_util.h"
#import "base/task/sequenced_task_runner.h"
#import "base/threading/thread_restrictions.h"
#import "build/branding_buildflags.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/web/features.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache_factory.h"
#import "ios/web/common/features.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Maximum size of session state NSData object in kilobyes.
const int64_t kMaxSessionState = 1024 * 5;  // 5MB

}  // anonymous namespace

// static
bool WebSessionStateTabHelper::IsEnabled() {
  if (!base::FeatureList::IsEnabled(web::kRestoreSessionFromCache)) {
    return false;
  }

  // This API is only available on iOS 15.
  if (@available(iOS 15, *)) {
    return true;
  }
  return false;
}

WebSessionStateTabHelper::WebSessionStateTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

WebSessionStateTabHelper::~WebSessionStateTabHelper() = default;

ChromeBrowserState* WebSessionStateTabHelper::GetBrowserState() {
  return ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
}

bool WebSessionStateTabHelper::RestoreSessionFromCache() {
  if (!IsEnabled())
    return false;

  WebSessionStateCache* cache =
      WebSessionStateCacheFactory::GetForBrowserState(GetBrowserState());
  NSData* data = [cache sessionStateDataForWebState:web_state_];
  if (!data.length)
    return false;

  bool restore_session_succeeded = web_state_->SetSessionStateData(data);
  UMA_HISTOGRAM_BOOLEAN("Session.WebStates.NativeRestoreSessionFromCache",
                        restore_session_succeeded);
  if (!restore_session_succeeded)
    return false;

  DCHECK(web_state_->GetNavigationItemCount());
  web::GetWebClient()->CleanupNativeRestoreURLs(web_state_);
  return true;
}

void WebSessionStateTabHelper::SaveSessionStateIfStale() {
  if (!stale_)
    return;
  SaveSessionState();
}

void WebSessionStateTabHelper::SaveSessionState() {
  if (!IsEnabled())
    return;

  stale_ = false;

  NSData* data = web_state_->SessionStateData();
  if (data) {
    int64_t size_kb = data.length / 1024;
    UMA_HISTOGRAM_COUNTS_100000("Session.WebState.CustomWebViewSerializedSize",
                                size_kb);

    WebSessionStateCache* cache =
        WebSessionStateCacheFactory::GetForBrowserState(GetBrowserState());
    // To prevent very large session states from using too much space, don't
    // persist any `data` larger than 5MB.  If this happens, remove the now
    // stale session state data.
    if (size_kb > kMaxSessionState) {
      [cache removeSessionStateDataForWebState:web_state_];
      return;
    }

    [cache persistSessionStateData:data forWebState:web_state_];
  }
}

#pragma mark - WebStateObserver

void WebSessionStateTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  if (stale_) {
    SaveSessionState();
  }
}

void WebSessionStateTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Don't record navigations that result in downloads, since these will be
  // discarded and there's no simple callback when discarded.
  if (navigation_context->IsDownload())
    return;

  MarkStale();
}

void WebSessionStateTabHelper::WebFrameDidBecomeAvailable(
    web::WebState* web_state,
    web::WebFrame* web_frame) {
  if (web_frame->IsMainFrame())
    return;

  // -WebFrameDidBecomeAvailable is called much more often than navigations, so
  // check if either `item_count_` or `last_committed_item_index_` has changed
  // before marking a page as stale.
  web::NavigationManager* navigation_manager =
      web_state->GetNavigationManager();
  if (item_count_ == web_state->GetNavigationItemCount() &&
      last_committed_item_index_ ==
          navigation_manager->GetLastCommittedItemIndex())
    return;

  MarkStale();
}

#pragma mark - Private

void WebSessionStateTabHelper::MarkStale() {
  if (!IsEnabled())
    return;

  web::NavigationManager* navigationManager =
      web_state_->GetNavigationManager();
  item_count_ = web_state_->GetNavigationItemCount();
  last_committed_item_index_ = navigationManager->GetLastCommittedItemIndex();

  stale_ = true;
}

WEB_STATE_USER_DATA_KEY_IMPL(WebSessionStateTabHelper)
