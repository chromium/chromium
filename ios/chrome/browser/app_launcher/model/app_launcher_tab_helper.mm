// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper.h"

#import <UIKit/UIKit.h>

#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/browser/url_blocklist_manager.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_abuse_detector.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper_browser_presentation_provider.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper_delegate.h"
#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_service.h"
#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_util.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/common/features.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_client.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

bool IsValidAppUrl(const GURL& app_url) {
  if (!app_url.is_valid()) {
    return false;
  }

  if (!app_url.has_scheme()) {
    return false;
  }

  // Block attempts to open this application's settings in the native system
  // settings application.
  if (app_url.SchemeIs("app-settings")) {
    return false;
  }
  return true;
}

// Returns True if `app_url` has a Chrome bundle URL scheme.
bool HasChromeAppScheme(const GURL& app_url) {
  NSArray* chrome_schemes =
      [[ChromeAppConstants sharedInstance] allBundleURLSchemes];
  NSString* app_url_scheme = base::SysUTF8ToNSString(app_url.scheme());
  return [chrome_schemes containsObject:app_url_scheme];
}

// This enum used by the Applauncher to log to UMA, if App launching request was
// allowed or blocked.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ExternalURLRequestStatus {
  kMainFrameRequestAllowed = 0,
  kSubFrameRequestAllowed = 1,
  kSubFrameRequestBlocked = 2,
  kCount,
};

// Execute the callbacks contained in `callbacks`.
void ExecuteCallbacks(std::vector<base::OnceClosure> callbacks) {
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

}  // namespace

AppLauncherTabHelper::AppLauncherTabHelper(
    web::WebState* web_state,
    AppLauncherAbuseDetector* abuse_detector,
    bool incognito)
    : web::WebStatePolicyDecider(web_state),
      web_state_(web_state),
      abuse_detector_(abuse_detector),
      incognito_(incognito) {
  DCHECK(abuse_detector_);
}

AppLauncherTabHelper::~AppLauncherTabHelper() = default;

// static
bool AppLauncherTabHelper::IsAppUrl(const GURL& url) {
  return !(web::UrlHasWebScheme(url) ||
           web::GetWebClient()->IsAppSpecificURL(url) ||
           url.SchemeIs(url::kFileScheme) || url.SchemeIs(url::kAboutScheme) ||
           url.SchemeIs(url::kBlobScheme));
}

void AppLauncherTabHelper::SetDelegate(AppLauncherTabHelperDelegate* delegate) {
  delegate_ = delegate;
}

void AppLauncherTabHelper::SetBrowserPresentationProvider(
    id<AppLauncherTabHelperBrowserPresentationProvider>
        browser_presentation_provider) {
  browser_presentation_provider_ = browser_presentation_provider;
}

void AppLauncherTabHelper::RequestToLaunchApp(const GURL& url,
                                              const GURL& source_page_url,
                                              bool link_transition,
                                              bool is_user_initiated,
                                              bool user_tapped_recently) {
  // Don't open external application if chrome is not active, or if the
  // web_state is not visible.
  if ([[UIApplication sharedApplication] applicationState] !=
          UIApplicationStateActive ||
      !web_state_->IsVisible() || !browser_presentation_provider_ ||
      [browser_presentation_provider_ isBrowserPresentingUI]) {
    return;
  }

  // Don't try to open external application if a prompt is already active or an
  // app launch request is already pending completion.
  if (is_prompt_active_ || is_app_launch_request_pending_) {
    return;
  }

  if (incognito_) {
    ShowAppLaunchAlert(AppLauncherAlertCause::kOpenFromIncognito, url);
    return;
  }

  if (!(is_user_initiated ||
        (url.SchemeIs(url::kTelScheme) && user_tapped_recently))) {
    ShowAppLaunchAlert(AppLauncherAlertCause::kNoUserInteraction, url);
    return;
  }

  // Show the a dialog for app store launches and external URL navigations that
  // did not originate from a link tap.
  if (UrlHasAppStoreScheme(url) || !link_transition) {
    ShowAppLaunchAlert(AppLauncherAlertCause::kOther, url);
    return;
  }

  [abuse_detector_ didRequestLaunchExternalAppURL:url
                                fromSourcePageURL:source_page_url];
  ExternalAppLaunchPolicy policy =
      [abuse_detector_ launchPolicyForURL:url
                        fromSourcePageURL:source_page_url];
  switch (policy) {
    case ExternalAppLaunchPolicyBlock: {
      return;
    }
    case ExternalAppLaunchPolicyAllow: {
      if (delegate_) {
        is_app_launch_request_pending_ = true;
        delegate_->LaunchAppForTabHelper(
            this, url,
            base::BindOnce(&AppLauncherTabHelper::OnAppLaunchCompleted,
                           weak_factory_.GetWeakPtr()),
            base::BindOnce(&AppLauncherTabHelper::AppNoLongerInactive,
                           weak_factory_.GetWeakPtr()));
      }
      return;
    }
    case ExternalAppLaunchPolicyPrompt: {
      ShowAppLaunchAlert(AppLauncherAlertCause::kRepeatedLaunchDetected, url);
      return;
    }
  }
}

void AppLauncherTabHelper::ShowAppLaunchAlert(AppLauncherAlertCause cause,
                                              const GURL& url) {
  if (!delegate_) {
    LaunchAppRequestCompleted();
    return;
  }
  is_prompt_active_ = true;
  delegate_->ShowAppLaunchAlert(
      this, cause,
      base::BindOnce(&AppLauncherTabHelper::OnShowAppLaunchAlertDone,
                     weak_factory_.GetWeakPtr(), url));
}

void AppLauncherTabHelper::OnShowAppLaunchAlertDone(const GURL& url,
                                                    bool user_allowed) {
  if (!user_allowed || !delegate_) {
    is_prompt_active_ = false;
    LaunchAppRequestCompleted();
    return;
  }

  is_app_launch_request_pending_ = true;
  delegate_->LaunchAppForTabHelper(
      this, url,
      base::BindOnce(&AppLauncherTabHelper::OnAppLaunchTried,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&AppLauncherTabHelper::AppNoLongerInactive,
                     weak_factory_.GetWeakPtr()));
}

void AppLauncherTabHelper::OnAppLaunchTried(bool success) {
  if (success) {
    return OnAppLaunchCompleted(success);
  }
  delegate_->ShowAppLaunchAlert(
      this, AppLauncherAlertCause::kAppLaunchFailed,
      base::BindOnce(&AppLauncherTabHelper::ShowFailureAlertDone,
                     weak_factory_.GetWeakPtr()));
}

void AppLauncherTabHelper::ShowFailureAlertDone(bool user_allowed) {
  return OnAppLaunchCompleted(false);
}

void AppLauncherTabHelper::OnAppLaunchCompleted(bool success) {
  if (success && !base::FeatureList::IsEnabled(
                     kInactiveNavigationAfterAppLaunchKillSwitch)) {
    return;
  }
  LaunchAppRequestCompleted();
}

void AppLauncherTabHelper::AppNoLongerInactive() {
  if (!base::FeatureList::IsEnabled(
          kInactiveNavigationAfterAppLaunchKillSwitch)) {
    LaunchAppRequestCompleted();
  }
}

void AppLauncherTabHelper::LaunchAppRequestCompleted() {
  is_app_launch_request_pending_ = false;
  is_prompt_active_ = false;

  // Some of the callback may destruct `this`, so post the execution to remove
  // the function from the stack.
  std::vector<base::OnceClosure> callbacks;
  std::swap(callbacks_waiting_for_app_launch_completion_, callbacks);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ExecuteCallbacks, std::move(callbacks)));
}

void AppLauncherTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  const auto policy_decision_and_optional_app_launch_request =
      GetPolicyDecisionAndOptionalAppLaunchRequest(request, request_info);
  const auto& policy_decision =
      policy_decision_and_optional_app_launch_request.first;
  if (policy_decision.ShouldAllowNavigation() &&
      (is_app_launch_request_pending_ || is_prompt_active_)) {
    callbacks_waiting_for_app_launch_completion_.push_back(
        base::BindOnce(std::move(callback), policy_decision));
    // No need to check any app launch request since it would be canceled by the
    // already ongoing app launch.
    return;
  }

  // If there is an app launch request, request the app launch now.
  if (policy_decision_and_optional_app_launch_request.second) {
    const AppLaunchRequest& app_launch_request =
        policy_decision_and_optional_app_launch_request.second.value();
    RequestToLaunchApp(app_launch_request.url,
                       app_launch_request.source_page_url,
                       app_launch_request.link_transition,
                       app_launch_request.is_user_initiated,
                       app_launch_request.user_tapped_recently);
  }

  if (!is_prompt_active_) {
    std::move(callback).Run(policy_decision);
  } else {
    callbacks_waiting_for_app_launch_completion_.push_back(
        base::BindOnce(std::move(callback), policy_decision));
  }
}

AppLauncherTabHelper::PolicyDecisionAndOptionalAppLaunchRequest
AppLauncherTabHelper::GetPolicyDecisionAndOptionalAppLaunchRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info) const {
  using PolicyDecision = web::WebStatePolicyDecider::PolicyDecision;
  static const std::optional<AppLaunchRequest> kNoAppLaunchRequest =
      std::nullopt;
  GURL request_url = net::GURLWithNSURL(request.URL);

  if (!IsAppUrl(request_url)) {
    // This URL can be handled by the WebState and doesn't require App launcher
    // handling.
    return {web::WebStatePolicyDecider::PolicyDecision::Allow(),
            kNoAppLaunchRequest};
  }

  // Do not allow allow navigation if URL is blocked by enterprise policy.
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state()->GetBrowserState());
  PolicyBlocklistService* blocklistService =
      PolicyBlocklistServiceFactory::GetForProfile(profile);
  if (blocklistService->GetURLBlocklistState(request_url) ==
      policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    return {PolicyDecision::CancelAndDisplayError(
                policy_url_blocking_util::CreateBlockedUrlError()),
            kNoAppLaunchRequest};
  }

  // Disallow navigations to tel: URLs from cross-origin frames.
  if (request_url.SchemeIs(url::kTelScheme) &&
      request_info.target_frame_is_cross_origin) {
    return {PolicyDecision::Cancel(), kNoAppLaunchRequest};
  }

  if (!base::FeatureList::IsEnabled(
          web::features::kAllowCrossWindowExternalAppNavigation) &&
      request_info.target_window_is_cross_origin) {
    return {PolicyDecision::Cancel(), kNoAppLaunchRequest};
  }

  // Disallow launching Chrome from within Chrome, as there are no good use
  // cases for this but allowing it opens the door to abuse.
  bool is_chrome_launch_attempt = HasChromeAppScheme(request_url);
  UMA_HISTOGRAM_BOOLEAN("IOS.AppLauncher.AppURLHasChromeLaunchScheme",
                        is_chrome_launch_attempt);
  if (is_chrome_launch_attempt) {
    return {PolicyDecision::Cancel(), kNoAppLaunchRequest};
  }

  ExternalURLRequestStatus request_status =
      ExternalURLRequestStatus::kMainFrameRequestAllowed;
  // TODO(crbug.com/40580645): Check if the source frame should also be
  // considered.
  if (!request_info.target_frame_is_main) {
    request_status = ExternalURLRequestStatus::kSubFrameRequestAllowed;
    // Don't allow navigations from iframe to apps if there is no user gesture.
    if (!request_info.is_user_initiated) {
      request_status = ExternalURLRequestStatus::kSubFrameRequestBlocked;
    }
  }

  // Request is blocked.
  if (request_status == ExternalURLRequestStatus::kSubFrameRequestBlocked) {
    return {PolicyDecision::Cancel(), kNoAppLaunchRequest};
  }

  if (!IsValidAppUrl(request_url)) {
    return {PolicyDecision::Cancel(), kNoAppLaunchRequest};
  }

  GURL last_committed_url = web_state_->GetLastCommittedURL();
  web::NavigationItem* pending_item =
      web_state_->GetNavigationManager()->GetPendingItem();
  GURL original_pending_url =
      pending_item ? pending_item->GetOriginalRequestURL() : GURL();
  bool is_link_transition = ui::PageTransitionCoreTypeIs(
      request_info.transition_type, ui::PAGE_TRANSITION_LINK);

  if (!is_link_transition && original_pending_url.is_valid()) {
    // At this stage the navigation will be canceled in all cases. If this
    // was a redirection, the `source_url` may not have been reported to
    // ReadingListWebStateObserver. Report it to mark as read if needed.
    ReadingListModel* model = ReadingListModelFactory::GetForProfile(profile);
    if (model && model->loaded()) {
      model->SetReadStatusIfExists(original_pending_url, true);
    }
  }
  std::optional<AppLaunchRequest> optional_app_launch_request =
      kNoAppLaunchRequest;
  if (last_committed_url.is_valid() ||
      !web_state_->GetNavigationManager()->GetLastCommittedItem()) {
    // Launch the app if the URL is valid or if it is the first page of the
    // tab.
    optional_app_launch_request = AppLaunchRequest{
        request_url, last_committed_url, is_link_transition,
        request_info.is_user_initiated, request_info.user_tapped_recently};
  }
  return {PolicyDecision::Cancel(), std::move(optional_app_launch_request)};
}

WEB_STATE_USER_DATA_KEY_IMPL(AppLauncherTabHelper)
