// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/web_session_state_tab_helper.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/logging.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/path_service.h"
#import "base/strings/string_util.h"
#import "base/task/sequenced_task_runner.h"
#import "base/threading/thread_restrictions.h"
#import "build/branding_buildflags.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/sessions/model/web_session_state_cache.h"
#import "ios/chrome/browser/sessions/model/web_session_state_cache_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Maximum size of session state NSData object in kilobyes.
const int64_t kMaxSessionState = 1024 * 5;  // 5MB

}  // anonymous namespace

// Observes scroll and zoom events and executes LoggingBlock.
@interface WebSessionStateScrollingObserver
    : NSObject <CRWWebViewScrollViewProxyObserver>
- (instancetype)initWithClosure:(base::RepeatingClosure)loggingClosure;

@end
@implementation WebSessionStateScrollingObserver {
  base::RepeatingClosure callback_;
}

- (instancetype)initWithClosure:(base::RepeatingClosure)closure {
  if ((self = [super init])) {
    callback_ = std::move(closure);
  }
  return self;
}

- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate {
  callback_.Run();
}

- (void)webViewScrollViewDidEndZooming:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                               atScale:(CGFloat)scale {
  callback_.Run();
}

@end

WebSessionStateTabHelper::WebSessionStateTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
  web_state_->GetPageWorldWebFramesManager()->AddObserver(this);
  if (web_state_->IsRealized()) {
    CreateScrollingObserver();
  }
}

WebSessionStateTabHelper::~WebSessionStateTabHelper() = default;

ProfileIOS* WebSessionStateTabHelper::GetProfile() {
  return ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
}

NSData* WebSessionStateTabHelper::FetchSessionFromCache() {
  WebSessionStateCache* cache =
      WebSessionStateCacheFactory::GetForProfile(GetProfile());
  NSData* data =
      [cache sessionStateDataForWebStateID:web_state_->GetUniqueIdentifier()];
  return data.length ? data : nil;
}

void WebSessionStateTabHelper::SaveSessionStateIfStale() {
  if (!stale_)
    return;
  SaveSessionState();
}

void WebSessionStateTabHelper::SaveSessionState() {
  stale_ = false;

  NSData* data = web_state_->SessionStateData();
  if (data) {
    int64_t size_kb = data.length / 1024;
    WebSessionStateCache* cache =
        WebSessionStateCacheFactory::GetForProfile(GetProfile());
    // To prevent very large session states from using too much space, don't
    // persist any `data` larger than 5MB.  If this happens, remove the now
    // stale session state data.
    if (size_kb > kMaxSessionState) {
      [cache
          removeSessionStateDataForWebStateID:web_state_->GetUniqueIdentifier()
                                    incognito:GetProfile()->IsOffTheRecord()];
      return;
    }

    [cache persistSessionStateData:data
                     forWebStateID:web_state_->GetUniqueIdentifier()];
  }
}

#pragma mark - WebStateObserver

void WebSessionStateTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  web_state_->GetPageWorldWebFramesManager()->RemoveObserver(this);
  if (scroll_observer_) {
    [web_state->GetWebViewProxy().scrollViewProxy
        removeObserver:scroll_observer_];
    scroll_observer_ = nil;
  }

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

void WebSessionStateTabHelper::WebFrameBecameAvailable(
    web::WebFramesManager* web_frames_manager,
    web::WebFrame* web_frame) {
  if (web_frame->IsMainFrame())
    return;

  // -WebFrameBecameAvailable is called much more often than navigations, so
  // check if either `item_count_` or `last_committed_item_index_` has changed
  // before marking a page as stale.
  web::NavigationManager* navigation_manager =
      web_state_->GetNavigationManager();
  if (item_count_ == web_state_->GetNavigationItemCount() &&
      last_committed_item_index_ ==
          navigation_manager->GetLastCommittedItemIndex()) {
    return;
  }

  MarkStale();
}

void WebSessionStateTabHelper::WebStateRealized(web::WebState* web_state) {
  CreateScrollingObserver();
}

#pragma mark - Private

void WebSessionStateTabHelper::CreateScrollingObserver() {
  base::RepeatingClosure closure = base::BindRepeating(
      &WebSessionStateTabHelper::OnScrollEvent, weak_ptr_factory_.GetWeakPtr());
  DCHECK(!scroll_observer_);
  scroll_observer_ =
      [[WebSessionStateScrollingObserver alloc] initWithClosure:closure];
  [web_state_->GetWebViewProxy().scrollViewProxy addObserver:scroll_observer_];
}

void WebSessionStateTabHelper::OnScrollEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stale_ = true;
}

void WebSessionStateTabHelper::MarkStale() {
  web::NavigationManager* navigationManager =
      web_state_->GetNavigationManager();
  item_count_ = web_state_->GetNavigationItemCount();
  last_committed_item_index_ = navigationManager->GetLastCommittedItemIndex();

  stale_ = true;
}

WEB_STATE_USER_DATA_KEY_IMPL(WebSessionStateTabHelper)
