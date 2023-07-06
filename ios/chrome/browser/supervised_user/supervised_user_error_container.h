// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ERROR_CONTAINER_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ERROR_CONTAINER_H_

#include <set>

#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "components/supervised_user/core/browser/supervised_user_interstitial.h"
#import "components/supervised_user/core/browser/supervised_user_service_observer.h"
#import "components/supervised_user/core/common/supervised_user_utils.h"
#import "ios/web/public/web_state_user_data.h"
#import "url/gurl.h"

namespace web {
class WebState;
}

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

  // Returns currently stored info associated with an error page.
  SupervisedUserErrorInfo& GetSupervisedUserErrorInfo();

  // Creates an instance of the SupervisedUserInterstitial from the
  // information stored in this container.
  void CreateSupervisedUserInterstitial();

  // Dispatch a supervised user interstitial command to the bound intersitial
  // for execution.
  void HandleCommand(
      supervised_user::SupervisedUserInterstitial::Commands command);

  // Return and move ownership of the SupervisedUserInterstitial instance.
  std::unique_ptr<supervised_user::SupervisedUserInterstitial>
  ReleaseSupervisedUserInterstitial() {
    return std::move(interstitial_);
  }

  // Checks if the host of the `url` has been already requested for approval.
  bool IsRemoteApprovalPendingForUrl(const GURL& url);

  // SupervisedUserServiceObserver override:
  void OnURLFilterChanged() override;

 private:
  friend class web::WebStateUserData<SupervisedUserErrorContainer>;

  explicit SupervisedUserErrorContainer(web::WebState* web_state);

  void OnRequestCreated(RequestUrlAccessRemoteCallback callback,
                        const std::string& host,
                        bool successfully_created_request);
  void MaybeUpdatePendingApprovals();

  WEB_STATE_USER_DATA_KEY_DECL();

  std::unique_ptr<SupervisedUserErrorInfo> supervised_user_error_info_;
  std::unique_ptr<supervised_user::SupervisedUserInterstitial> interstitial_;
  raw_ref<supervised_user::SupervisedUserService> supervised_user_service_;
  raw_ptr<web::WebState> web_state_;
  std::set<std::string> requested_hosts_;

  base::WeakPtrFactory<SupervisedUserErrorContainer> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ERROR_CONTAINER_H_
