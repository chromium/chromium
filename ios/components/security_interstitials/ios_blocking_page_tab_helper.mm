// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"

#import "base/logging.h"
#import "base/strings/string_number_conversions.h"
#import "base/values.h"
#import "ios/components/security_interstitials/ios_security_interstitial_page.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_user_data.h"

namespace security_interstitials {

WEB_STATE_USER_DATA_KEY_IMPL(IOSBlockingPageTabHelper)

#pragma mark - IOSBlockingPageTabHelper

IOSBlockingPageTabHelper::IOSBlockingPageTabHelper(web::WebState* web_state)
    : navigation_id_listener_(web_state, this) {}

IOSBlockingPageTabHelper::~IOSBlockingPageTabHelper() = default;

void IOSBlockingPageTabHelper::AssociateBlockingPage(
    int64_t navigation_id,
    std::unique_ptr<IOSSecurityInterstitialPage> blocking_page) {
  if (navigation_id == last_committed_navigation_id_) {
    blocking_page_for_currently_committed_navigation_ =
        std::move(blocking_page);
  } else {
    blocking_pages_for_navigations_[navigation_id] = std::move(blocking_page);
  }
}

bool IOSBlockingPageTabHelper::ShouldDisplayURL() const {
  return blocking_page_for_currently_committed_navigation_->ShouldDisplayURL();
}

IOSSecurityInterstitialPage* IOSBlockingPageTabHelper::GetCurrentBlockingPage()
    const {
  return blocking_page_for_currently_committed_navigation_.get();
}

void IOSBlockingPageTabHelper::OnBlockingPageCommandReceived(
    SecurityInterstitialCommand command) {
  if (!blocking_page_for_currently_committed_navigation_)
    return;

  blocking_page_for_currently_committed_navigation_->HandleCommand(command);
}

void IOSBlockingPageTabHelper::UpdateForFinishedNavigation(
    int64_t navigation_id,
    bool committed) {
  if (committed) {
    last_committed_navigation_id_ = navigation_id;
    blocking_page_for_currently_committed_navigation_ =
        std::move(blocking_pages_for_navigations_[navigation_id]);
  }
  blocking_pages_for_navigations_.erase(navigation_id);
}

#pragma mark - IOSBlockingPageTabHelper::CommittedNavigationIDListener

IOSBlockingPageTabHelper::CommittedNavigationIDListener::
    CommittedNavigationIDListener(web::WebState* web_state,
                                  IOSBlockingPageTabHelper* tab_helper)
    : tab_helper_(tab_helper) {
  DCHECK(tab_helper_);
  scoped_observation_.Observe(web_state);
}

IOSBlockingPageTabHelper::CommittedNavigationIDListener::
    ~CommittedNavigationIDListener() = default;

void IOSBlockingPageTabHelper::CommittedNavigationIDListener::
    DidFinishNavigation(web::WebState* web_state,
                        web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument())
    return;

  tab_helper_->UpdateForFinishedNavigation(
      navigation_context->GetNavigationId(),
      navigation_context->HasCommitted());

  // Interstitials may change the visibility of the URL or other security state.
  web_state->DidChangeVisibleSecurityState();

  IOSSecurityInterstitialPage* page = tab_helper_->GetCurrentBlockingPage();
  if (!page) {
    // `page` will be null if a IOSSecurityInterstitialPage is not being
    // displayed to the user.
    return;
  }
  page->ShowInfobar();
}

void IOSBlockingPageTabHelper::CommittedNavigationIDListener::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK(scoped_observation_.IsObservingSource(web_state));
  scoped_observation_.Reset();
}

}  // namespace security_interstitials
