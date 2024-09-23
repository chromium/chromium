// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_TAB_HELPER_H_

#import <optional>

#import "base/memory/raw_ptr.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

@class AppLauncherAbuseDetector;
enum class AppLauncherAlertCause;
@protocol AppLauncherTabHelperBrowserPresentationProvider;
class AppLauncherTabHelperDelegate;
class GURL;

// A tab helper that handles requests to launch another application.
class AppLauncherTabHelper
    : public web::WebStatePolicyDecider,
      public web::WebStateUserData<AppLauncherTabHelper> {
 public:
  AppLauncherTabHelper(const AppLauncherTabHelper&) = delete;
  AppLauncherTabHelper& operator=(const AppLauncherTabHelper&) = delete;

  ~AppLauncherTabHelper() override;

  // Returns true, if the `url` has a scheme for an external application
  // (eg. twitter:// , calshow://).
  static bool IsAppUrl(const GURL& url);

  // Sets the delegate.
  virtual void SetDelegate(AppLauncherTabHelperDelegate* delegate);

  // Sets the provider to retrieve the browser presentation state.
  void SetBrowserPresentationProvider(
      id<AppLauncherTabHelperBrowserPresentationProvider>
          browser_presentation_provider);

  // Requests to open the application with `url`.
  // The method checks if the application for `url` has been opened repeatedly
  // by the `source_page_url` page in a short time frame, in that case a prompt
  // will appear to the user with an option to block the application from
  // launching. Then the method also checks for user interaction and for schemes
  // that require special handling (eg. facetime, mailto) and may present the
  // user with a confirmation dialog to open the application.
  void RequestToLaunchApp(const GURL& url,
                          const GURL& source_page_url,
                          bool link_transition,
                          bool is_user_initiated,
                          bool user_tapped_recently);

  // web::WebStatePolicyDecider implementation
  void ShouldAllowRequest(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;

 protected:
  // Constructor for AppLauncherTabHelper. `abuse_detector` provides policy for
  // launching apps.
  // Protected to allow test overriding.
  AppLauncherTabHelper(web::WebState* web_state,
                       AppLauncherAbuseDetector* abuse_detector,
                       bool incognito);

 private:
  friend class web::WebStateUserData<AppLauncherTabHelper>;

  // Getter for the delegate.
  AppLauncherTabHelperDelegate* delegate() const { return delegate_; }

  // Callback to AppLauncherTabHelperDelegate::LaunchAppForTabHelper when a
  // prompt was previously shown to the user.
  // If the launch was not successful, another prompt can be shown.
  void OnAppLaunchTried(bool success);

  // Called by the delegate when the failure prompt was shown to the user.
  // `user_allowed` is ignored as there is only one button.
  void ShowFailureAlertDone(bool user_allowed);

  // Called when external app is launched.
  void OnAppLaunchCompleted(bool success);

  // If app was launched successfully, called when application leaves the
  // the foreground inactive state.
  void AppNoLongerInactive();

  // Resets `is_app_launch_request_pending_` to `false` and call all callbacks
  // waiting for app completion.
  void LaunchAppRequestCompleted();

  // Triggers an in tab prompt to ask the user if the app launch should proceed.
  void ShowAppLaunchAlert(AppLauncherAlertCause cause, const GURL& url);

  // Called by the delegate once the user has been prompted. If `user_allowed`,
  // then `LaunchAppForTabHelper()` will be called on the delegate.
  void OnShowAppLaunchAlertDone(const GURL& url, bool user_allowed);

  // Holds the necessary data for a call to `RequestToLaunchApp()`. A value for
  // this type can optionally be returned by
  // `GetPolicyDecisionAndOptionalAppLaunchRequest()`.
  struct AppLaunchRequest {
    GURL url;
    GURL source_page_url;
    bool link_transition;
    bool is_user_initiated;
    bool user_tapped_recently;
  };
  using PolicyDecisionAndOptionalAppLaunchRequest =
      std::pair<web::WebStatePolicyDecider::PolicyDecision,
                std::optional<AppLaunchRequest>>;
  // Returns the appropriate policy decision for the given `request`. If the
  // request should trigger an app launch request, returns an app launch request
  // too.
  PolicyDecisionAndOptionalAppLaunchRequest
  GetPolicyDecisionAndOptionalAppLaunchRequest(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info) const;

  // The WebState that this object is attached to.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // Used to check for repeated launches and provide policy for launching apps.
  AppLauncherAbuseDetector* abuse_detector_ = nil;

  // Whether this TabHelper is in incognito.
  bool incognito_ = false;

  // Used to launch apps and present UI.
  raw_ptr<AppLauncherTabHelperDelegate> delegate_ = nullptr;

  // Used to know if the browser is currently presenting another VC.
  __weak id<AppLauncherTabHelperBrowserPresentationProvider>
      browser_presentation_provider_ = nil;

  // Returns whether there is a prompt shown by `RequestToOpenUrl` or not.
  bool is_prompt_active_ = false;

  // Whether there is an app launch request pending. Set to `true` before
  // calling `LaunchAppForTabHelper()` on the delegate.
  bool is_app_launch_request_pending_ = false;

  // Stores callbacks which should be called once the ongoing app launch
  // completes. When `ShouldAllowRequest()` asks this tab helper for a policy
  // decision, the call to the policy decision callback might need to be delayed
  // in case an app launch is still ongoing for this tab. These callbacks would
  // then be stored here until `AppLaunchCompleted()` is called. Then all
  // callbacks will run and `callbacks_waiting_for_app_launch_completion_` will
  // be cleared.
  // Note: Some of these callbacks may trigger a new app launch call or even
  // close the tab (and hence delete this TabHelper).
  std::vector<base::OnceClosure> callbacks_waiting_for_app_launch_completion_;

  // Must be last member to ensure it is destroyed last.
  base::WeakPtrFactory<AppLauncherTabHelper> weak_factory_{this};

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_TAB_HELPER_H_
