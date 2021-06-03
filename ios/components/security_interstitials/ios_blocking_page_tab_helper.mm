// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"

#include "base/logging.h"
#include "base/values.h"
#include "ios/components/security_interstitials/ios_security_interstitial_page.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_user_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace security_interstitials {

WEB_STATE_USER_DATA_KEY_IMPL(IOSBlockingPageTabHelper)

namespace {
// Script command prefix.
const char kCommandPrefix[] = "blockingPage";
}  // namespace

#pragma mark - IOSBlockingPageTabHelper

IOSBlockingPageTabHelper::IOSBlockingPageTabHelper(web::WebState* web_state)
    : navigation_id_listener_(web_state, this) {
  auto command_callback =
      base::BindRepeating(&IOSBlockingPageTabHelper::OnBlockingPageCommand,
                          weak_factory_.GetWeakPtr());
  subscription_ =
      web_state->AddScriptCommandCallback(command_callback, kCommandPrefix);
}

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

void IOSBlockingPageTabHelper::OnBlockingPageCommand(
    const base::Value& message,
    const GURL& url,
    bool user_is_interacting,
    web::WebFrame* sender_frame) {
  const std::string* command = message.FindStringKey("command");
  if (!command) {
    DLOG(WARNING) << "JS message parameter not found: command";
  } else {
    if (blocking_page_for_currently_committed_navigation_) {
      blocking_page_for_currently_committed_navigation_->HandleScriptCommand(
          base::Value::AsDictionaryValue(message), url, user_is_interacting,
          sender_frame);
    }
  }
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
}

void IOSBlockingPageTabHelper::CommittedNavigationIDListener::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK(scoped_observation_.IsObservingSource(web_state));
  scoped_observation_.Reset();
}

}  // namespace security_interstitials
