// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_ERROR_CONTAINER_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_ERROR_CONTAINER_H_

#include <set>

#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "components/security_interstitials/core/controller_client.h"
#import "components/supervised_user/core/browser/supervised_user_interstitial.h"
#import "components/supervised_user/core/browser/supervised_user_service_observer.h"
#import "components/supervised_user/core/browser/supervised_user_url_filter.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#import "ios/components/security_interstitials/ios_security_interstitial_page.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "url/gurl.h"

namespace web {
class WebState;
}

// Exposed for testing. It allows us to confirm that a Supervised User
// interstitial is created for a web state.
extern const char kSupervisedUserInterstitialType[];

using RequestUrlAccessRemoteCallback = base::OnceCallback<void(bool)>;

// Helper object that holds information needed for the supervised user
// interstitial functionality and error page.
class SupervisedUserErrorContainer
    : public web::WebStateUserData<SupervisedUserErrorContainer>,
      public SupervisedUserServiceObserver {
 public:
  SupervisedUserErrorContainer(SupervisedUserErrorContainer& other);
  SupervisedUserErrorContainer& operator=(SupervisedUserErrorContainer& other);
  ~SupervisedUserErrorContainer() override;

  // Structure that contains information for the supervised user interstitial
  // error page UI.
  class SupervisedUserErrorInfo {
   public:
    SupervisedUserErrorInfo(
        const GURL& request_url,
        bool is_main_frame,
        supervised_user::FilteringBehaviorReason filtering_behavior_reason);
    SupervisedUserErrorInfo() = delete;
    SupervisedUserErrorInfo(const SupervisedUserErrorInfo& other) = delete;
    SupervisedUserErrorInfo& operator=(const SupervisedUserErrorInfo& other) =
        delete;
    bool is_main_frame() const { return is_main_frame_; }
    supervised_user::FilteringBehaviorReason filtering_behavior_reason() const {
      return filtering_behavior_reason_;
    }
    GURL request_url() const { return request_url_; }

   private:
    bool is_main_frame_;
    supervised_user::FilteringBehaviorReason filtering_behavior_reason_;
    GURL request_url_;
  };

  // Stores info associated with a supervised user interstitial error page.
  void SetSupervisedUserErrorInfo(
      std::unique_ptr<SupervisedUserErrorInfo> error_info);

  // Returns and moves ownership of currently stored info associated with an
  // error page.
  std::unique_ptr<SupervisedUserErrorInfo> ReleaseSupervisedUserErrorInfo() {
    return std::move(supervised_user_error_info_);
  }

  // Creates an instance of the SupervisedUserInterstitial from the
  // provided `error_info`.
  std::unique_ptr<supervised_user::SupervisedUserInterstitial>
  CreateSupervisedUserInterstitial(SupervisedUserErrorInfo& error_info);

  // Dispatches a supervised user interstitial command to the given
  // `interstitial` for execution.
  void HandleCommand(
      supervised_user::SupervisedUserInterstitial& interstitial,
      security_interstitials::SecurityInterstitialCommand command);

  // Checks if the `url` host has been already requested for approval.
  bool IsRemoteApprovalPendingForUrl(const GURL& url);

  // SupervisedUserServiceObserver override:
  void OnURLFilterChanged() override;

 private:
  friend class web::WebStateUserData<SupervisedUserErrorContainer>;

  explicit SupervisedUserErrorContainer(web::WebState* web_state);

  void OnRequestCreated(RequestUrlAccessRemoteCallback callback,
                        const GURL& url,
                        bool successfully_created_request);
  void MaybeUpdatePendingApprovals();
  void URLFilterCheckCallback(const GURL& url,
                              supervised_user::FilteringBehavior behavior,
                              supervised_user::FilteringBehaviorReason reason,
                              bool uncertain);
  WEB_STATE_USER_DATA_KEY_DECL();

  std::unique_ptr<SupervisedUserErrorInfo> supervised_user_error_info_;
  raw_ref<supervised_user::SupervisedUserService> supervised_user_service_;
  raw_ptr<web::WebState> web_state_;
  std::set<std::string> requested_hosts_;
  base::WeakPtrFactory<SupervisedUserErrorContainer> weak_ptr_factory_{this};
};

// Wrapper class for the supervised user interstitial instance. It allows use to
// manage the interstitial's lifecysle through `IOSBlockingPageTabHelper`.
class SupervisedUserInterstitialBlockingPage
    : public security_interstitials::IOSSecurityInterstitialPage,
      public web::WebStateObserver {
 public:
  SupervisedUserInterstitialBlockingPage(
      std::unique_ptr<supervised_user::SupervisedUserInterstitial> interstitial,
      std::unique_ptr<security_interstitials::IOSBlockingPageControllerClient>
          controller_client,
      SupervisedUserErrorContainer* error_container,
      web::WebState* web_state);
  ~SupervisedUserInterstitialBlockingPage() override;
  supervised_user::SupervisedUserInterstitial& interstitial() {
    return *interstitial_;
  }

 private:
  // security_interstitials::IOSSecurityInterstitialPage implementation:
  void HandleCommand(
      security_interstitials::SecurityInterstitialCommand command) override;
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::Value::Dict& load_time_data) const override;
  std::string_view GetInterstitialType() const override;

  // web::WebStateObserver implementation:
  void WebStateDestroyed(web::WebState* web_state) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;

  // Marks the SU interstitial first time banner as shown for a visible page.
  void MaybeUpdateFirstTimeInterstitialBanner();

  const std::unique_ptr<supervised_user::SupervisedUserInterstitial>
      interstitial_;
  std::unique_ptr<security_interstitials::IOSBlockingPageControllerClient>
      controller_client_;
  raw_ptr<web::WebState> web_state_;
  raw_ptr<SupervisedUserErrorContainer> error_container_;
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_ERROR_CONTAINER_H_
