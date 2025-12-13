// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_DATA_SHARING_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_DATA_SHARING_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "ios/web/public/navigation/web_state_policy_decider.h"
#include "ios/web/public/web_state_user_data.h"

class DataSharingTabHelperDelegate;

// A tab helper that intercepts the navigation if the request's URL is for data
// sharing.
class DataSharingTabHelper
    : public web::WebStatePolicyDecider,
      public web::WebStateUserData<DataSharingTabHelper> {
 public:
  ~DataSharingTabHelper() override;

  DataSharingTabHelper(const DataSharingTabHelper&) = delete;
  DataSharingTabHelper& operator=(const DataSharingTabHelper&) = delete;

  // Sets the delegate.
  void SetDelegate(DataSharingTabHelperDelegate* delegate);

  // web::WebStatePolicyDecider implementation
  void ShouldAllowRequest(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;

 private:
  explicit DataSharingTabHelper(web::WebState* web_state);
  friend class web::WebStateUserData<DataSharingTabHelper>;

  // Returns the request for `url` should be intercepted.
  bool ShouldInterceptRequestforUrl(const GURL& url) const;

  raw_ptr<DataSharingTabHelperDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_DATA_SHARING_TAB_HELPER_H_
