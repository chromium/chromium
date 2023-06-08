// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/supervised_user_url_filter_tab_helper.h"

#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/supervised_user/core/browser/supervised_user_interstitial.h"
#import "components/supervised_user/core/common/pref_names.h"
#import "components/supervised_user/core/common/supervised_user_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/supervised_user/supervised_user_error.h"
#import "ios/chrome/browser/supervised_user/supervised_user_error_container.h"
#import "ios/net/protocol_handler_util.h"
#import "net/base/mac/url_conversions.h"
#import "net/base/net_errors.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kFilteredURLExample[] = "/filtered";

// Fake supervised user custodian information displayed in the interstitial.
// They will be fetched by the child account service once it is migrated to
// components.
void CreateMockData(web::WebState* web_state) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());

  browser_state->GetPrefs()->SetString(prefs::kSupervisedUserCustodianEmail,
                                       "primary@gmail.com");
  browser_state->GetPrefs()->SetString(prefs::kSupervisedUserCustodianName,
                                       "Primary Name");
  browser_state->GetPrefs()->SetString(
      prefs::kSupervisedUserSecondCustodianName, "Secondary Name");
  browser_state->GetPrefs()->SetString(
      prefs::kSupervisedUserSecondCustodianEmail, "secondary@gmail.com");
  browser_state->GetPrefs()->SetString(
      prefs::kSupervisedUserCustodianProfileImageURL, "thisurl.com");
  browser_state->GetPrefs()->SetString(
      prefs::kSupervisedUserSecondCustodianProfileImageURL, "otherurl.com");
}

}  // namespace

SupervisedUserURLFilterTabHelper::~SupervisedUserURLFilterTabHelper() = default;

SupervisedUserURLFilterTabHelper::SupervisedUserURLFilterTabHelper(
    web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

void SupervisedUserURLFilterTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  // TODO(b/265761985): integrate with SupervisedUserService::GetURLFilter().
  if ([request.URL.absoluteString containsString:@(kFilteredURLExample)]) {
    supervised_user::FilteringBehaviorReason reason =
        supervised_user::FilteringBehaviorReason::
            DEFAULT;  // TODO(b/265761985): Extract reason from filtering.
    // TODO(b/279765349): Remove once we have real data to populate the
    // interstitial.
    CreateMockData(web_state());

    SupervisedUserErrorContainer* container =
        SupervisedUserErrorContainer::FromWebState(web_state());
    CHECK(container);
    container->SetSupervisedUserErrorInfo(
        std::make_unique<SupervisedUserErrorContainer::SupervisedUserErrorInfo>(
            GURL(base::SysNSStringToUTF8(request.URL.absoluteString)),
            request_info.target_frame_is_main,
            // TODO(b/265761985): Update once we have this information.
            /*is_already_requested=*/false, reason));

    std::move(callback).Run(CreateSupervisedUserInterstitialErrorDecision());
    return;
  }

  std::move(callback).Run(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

WEB_STATE_USER_DATA_KEY_IMPL(SupervisedUserURLFilterTabHelper)
