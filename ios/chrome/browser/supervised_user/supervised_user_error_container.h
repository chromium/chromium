// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ERROR_CONTAINER_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ERROR_CONTAINER_H_

#import "components/supervised_user/core/common/supervised_user_utils.h"
#import "ios/web/public/web_state_user_data.h"
#import "url/gurl.h"

namespace web {
class WebState;
}

// Helper object that holds information needed for the supervised user
// interstitial functionality and error page.
class SupervisedUserErrorContainer
    : public web::WebStateUserData<SupervisedUserErrorContainer> {
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
        bool is_already_requested,
        supervised_user::FilteringBehaviorReason filtering_behavior_reason);
    SupervisedUserErrorInfo() = delete;
    SupervisedUserErrorInfo(const SupervisedUserErrorInfo& other) = delete;
    SupervisedUserErrorInfo& operator=(const SupervisedUserErrorInfo& other) =
        delete;
    bool is_main_frame() const { return is_main_frame_; }
    bool is_already_requested() const { return is_already_requested_; }
    supervised_user::FilteringBehaviorReason filtering_behavior_reason() const {
      return filtering_behavior_reason_;
    }
    GURL request_url() const { return request_url_; }

   private:
    bool is_main_frame_;
    bool is_already_requested_;
    supervised_user::FilteringBehaviorReason filtering_behavior_reason_;
    GURL request_url_;
  };

  // Stores info associated with a supervised user interstitial error page.
  void SetSupervisedUserErrorInfo(
      std::unique_ptr<SupervisedUserErrorInfo> error_info);

  // Returns currently stored info associated with an error page.
  SupervisedUserErrorInfo& GetSupervisedUserErrorInfo();

 private:
  friend class web::WebStateUserData<SupervisedUserErrorContainer>;

  explicit SupervisedUserErrorContainer(web::WebState* web_state);

  WEB_STATE_USER_DATA_KEY_DECL();

  std::unique_ptr<SupervisedUserErrorInfo> supervised_user_error_info_;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_ERROR_CONTAINER_H_
