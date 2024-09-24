// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_error_container.h"

#import <string>

#import "base/memory/ptr_util.h"
#import "base/notreached.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "components/supervised_user/core/browser/supervised_user_url_filter.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/supervised_user/model/ios_web_content_handler_impl.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

WEB_STATE_USER_DATA_KEY_IMPL(SupervisedUserErrorContainer)

namespace {

const char* BoolToString(bool value) {
  return value ? "true" : "false";
}

// Updates the request status on the Interstitial upon making
// a permission request.
// The method is invoked as a callback, so it is recommended to
// bind a weak pointer to the webstate, in case it has been invalidated.
void OnRequestUrlAccessRemote(base::WeakPtr<web::WebState> weak_web_state,
                              bool is_main_frame,
                              bool is_request_successful) {
  web::WebState* web_state = weak_web_state.get();
  if (!web_state) {
    return;
  }
  NSString* js_to_execute =
      [NSString stringWithFormat:@"setRequestStatus(%s, %s)",
                                 BoolToString(is_request_successful),
                                 BoolToString(is_main_frame)];
  // Trigger Intersitial JS method.
  web_state->ExecuteUserJavaScript(js_to_execute);
}

}  // namespace

const char kSupervisedUserInterstitialType[] = "kSupervisedUserInterstitial";

SupervisedUserErrorContainer::SupervisedUserErrorContainer(
    web::WebState* web_state)
    : supervised_user_service_(*SupervisedUserServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state->GetBrowserState()))),
      web_state_(web_state) {
  CHECK(SupervisedUserServiceFactory::GetForProfile(
      ProfileIOS::FromBrowserState(web_state->GetBrowserState())));
  supervised_user_service_->AddObserver(this);
}

SupervisedUserErrorContainer::~SupervisedUserErrorContainer() {
  supervised_user_service_->RemoveObserver(this);
}

SupervisedUserErrorContainer::SupervisedUserErrorInfo::SupervisedUserErrorInfo(
    const GURL& request_url,
    bool is_main_frame,
    supervised_user::FilteringBehaviorReason filtering_behavior_reason) {
  request_url_ = request_url;
  is_main_frame_ = is_main_frame;
  filtering_behavior_reason_ = filtering_behavior_reason;
}
void SupervisedUserErrorContainer::SetSupervisedUserErrorInfo(
    std::unique_ptr<SupervisedUserErrorInfo> error_info) {
  supervised_user_error_info_ = std::move(error_info);
}

std::unique_ptr<supervised_user::SupervisedUserInterstitial>
SupervisedUserErrorContainer::CreateSupervisedUserInterstitial(
    SupervisedUserErrorInfo& error_info) {
  std::unique_ptr<IOSWebContentHandlerImpl> web_content_handler =
      std::make_unique<IOSWebContentHandlerImpl>(web_state_,
                                                 error_info.is_main_frame());

  std::unique_ptr<supervised_user::SupervisedUserInterstitial> interstitial =
      supervised_user::SupervisedUserInterstitial::Create(
          std::move(web_content_handler), supervised_user_service_.get(),
          error_info.request_url(),
          // User name needed only for the local web approval flow, not
          // applicable for iOS.
          /*supervised_user_name=*/std::u16string(),
          error_info.filtering_behavior_reason());
  return interstitial;
}

void SupervisedUserErrorContainer::HandleCommand(
    supervised_user::SupervisedUserInterstitial& interstitial,
    security_interstitials::SecurityInterstitialCommand command) {
  if (command == security_interstitials::SecurityInterstitialCommand::
                     CMD_REQUEST_SITE_ACCESS_PERMISSION) {
    RequestUrlAccessRemoteCallback callback =
        base::BindOnce(&OnRequestUrlAccessRemote, web_state_->GetWeakPtr(),
                       interstitial.web_content_handler()->IsMainFrame());
    interstitial.RequestUrlAccessRemote(
        base::BindOnce(&SupervisedUserErrorContainer::OnRequestCreated,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       interstitial.url()));
  } else if (command == security_interstitials::SecurityInterstitialCommand::
                            CMD_DONT_PROCEED) {
    interstitial.GoBack();
  }
}

bool SupervisedUserErrorContainer::IsRemoteApprovalPendingForUrl(
    const GURL& url) {
  return base::Contains(requested_hosts_, url.host());
}

void SupervisedUserErrorContainer::URLFilterCheckCallback(
    const GURL& url,
    supervised_user::FilteringBehavior behavior,
    supervised_user::FilteringBehaviorReason reason,
    bool uncertain) {
  auto* blocking_tab_helper =
      security_interstitials::IOSBlockingPageTabHelper::FromWebState(
          web_state_);
  CHECK(blocking_tab_helper);
  security_interstitials::IOSSecurityInterstitialPage* blocking_page =
      blocking_tab_helper->GetCurrentBlockingPage();

  // Early exit if the blocking page is not a supervised user interstitial.
  if (blocking_page &&
      blocking_page->GetInterstitialType() != kSupervisedUserInterstitialType) {
    return;
  }

  bool is_showing_supervised_user_interstitial_for_url = false;
  bool is_main_frame = true;

  if (blocking_page) {
    // If a blocking_page exists here, then it has the right type.
    SupervisedUserInterstitialBlockingPage* supervised_user_blocking_page =
        static_cast<SupervisedUserInterstitialBlockingPage*>(blocking_page);
    is_showing_supervised_user_interstitial_for_url =
        supervised_user_blocking_page->interstitial().url() == url;
    is_main_frame = supervised_user_blocking_page->interstitial()
                        .web_content_handler()
                        ->IsMainFrame();
  }

  bool should_show_interstitial =
      behavior == supervised_user::FilteringBehavior::kBlock;

  if (is_showing_supervised_user_interstitial_for_url !=
      should_show_interstitial) {
    // The present interstitial framework on iOS supports main frames only.
    // It it is not possible to obtain or refresh a subframe interstitial.
    if (is_main_frame && web_state_->IsRealized()) {
      web_state_->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                                 /*check_for_repost=*/true);
    }
  }
}

void SupervisedUserErrorContainer::OnURLFilterChanged() {
  supervised_user_service_->GetURLFilter()
      ->GetFilteringBehaviorForURLWithAsyncChecks(
          web_state_->GetLastCommittedURL(),
          base::BindOnce(&SupervisedUserErrorContainer::URLFilterCheckCallback,
                         weak_ptr_factory_.GetWeakPtr(),
                         web_state_->GetLastCommittedURL()),
          /*skip_manual_parent_filter=*/false);

  MaybeUpdatePendingApprovals();
}

void SupervisedUserErrorContainer::OnRequestCreated(
    RequestUrlAccessRemoteCallback callback,
    const GURL& url,
    bool successfully_created_request) {
  if (successfully_created_request) {
    requested_hosts_.insert(url.host());
  }
  std::move(callback).Run(successfully_created_request);
}

void SupervisedUserErrorContainer::MaybeUpdatePendingApprovals() {
  supervised_user::FilteringBehavior filtering_behavior;
  supervised_user::SupervisedUserURLFilter* url_filter =
      supervised_user_service_->GetURLFilter();

  for (auto iter = requested_hosts_.begin(); iter != requested_hosts_.end();) {
    bool is_manual = url_filter->GetManualFilteringBehaviorForURL(
        GURL(*iter), &filtering_behavior);

    if (is_manual &&
        filtering_behavior == supervised_user::FilteringBehavior::kAllow) {
      iter = requested_hosts_.erase(iter);
    } else {
      iter++;
    }
  }
}

SupervisedUserInterstitialBlockingPage::SupervisedUserInterstitialBlockingPage(
    std::unique_ptr<supervised_user::SupervisedUserInterstitial> interstitial,
    std::unique_ptr<security_interstitials::IOSBlockingPageControllerClient>
        controller_client,
    SupervisedUserErrorContainer* error_container,
    web::WebState* web_state)
    : security_interstitials::IOSSecurityInterstitialPage(
          web_state,
          interstitial->url(),
          controller_client.get()),
      interstitial_(std::move(interstitial)),
      controller_client_(std::move(controller_client)),
      web_state_(web_state),
      error_container_(error_container) {
  CHECK(interstitial_);
  scoped_observation_.Observe(web_state);
}

SupervisedUserInterstitialBlockingPage::
    ~SupervisedUserInterstitialBlockingPage() = default;

void SupervisedUserInterstitialBlockingPage::HandleCommand(
    security_interstitials::SecurityInterstitialCommand command) {
  CHECK(error_container_);
  error_container_->HandleCommand(*interstitial_, command);

  // If the page was pre-rendered, the first time banner was not marked
  // on page loading.
  MaybeUpdateFirstTimeInterstitialBanner();
}

bool SupervisedUserInterstitialBlockingPage::ShouldCreateNewNavigation() const {
  NOTREACHED();
}

void SupervisedUserInterstitialBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) const {
  NOTREACHED();
}

std::string_view SupervisedUserInterstitialBlockingPage::GetInterstitialType()
    const {
  return kSupervisedUserInterstitialType;
}

// Note: The SupervisedUserInterstitialBlockingPage has a pointer to
// error_container_ (which is WebStateUserData helper) and is managed by
// SupervisedUserInterstitialBlockingPage (another WebStateUserData helper).
// The order of their destruction is unspecified, so the present object
// observes the `web_state` to reset the pointer.
void SupervisedUserInterstitialBlockingPage::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK(scoped_observation_.IsObservingSource(web_state));
  error_container_ = nullptr;
  scoped_observation_.Reset();
}

void SupervisedUserInterstitialBlockingPage::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  MaybeUpdateFirstTimeInterstitialBanner();
}

void SupervisedUserInterstitialBlockingPage::
    MaybeUpdateFirstTimeInterstitialBanner() {
  if (!interstitial_->web_content_handler()->IsMainFrame()) {
    return;
  }
  if (!web_state_->IsVisible()) {
    // Only mark the banner if the loaded page is visible (it might be
    // pre-rendered).
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  CHECK(profile);
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);

  CHECK(supervised_user_service);
  supervised_user_service->MarkFirstTimeInterstitialBannerShown();
}
