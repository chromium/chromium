// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_TAB_HELPER_H_

#import "base/functional/callback.h"
#import "ios/web/public/lazy_web_state_user_data.h"

namespace web {
class WebState;
}

namespace data_controls {

// Manages Enterprise Data Control policies for the associated tab. These
// policies determine whether certain user actions, like clipboard operations
// (copying, pasting), are permitted. Such restrictions only apply to managed
// profiles; for all other profiles, these actions are unrestricted.
class DataControlsTabHelper
    : public web::LazyWebStateUserData<DataControlsTabHelper> {
 public:
  DataControlsTabHelper(const DataControlsTabHelper&) = delete;
  DataControlsTabHelper& operator=(const DataControlsTabHelper&) = delete;
  ~DataControlsTabHelper() override;

  // Determines if copying should be allowed.
  void ShouldAllowCopy(base::OnceCallback<void(bool)> callback);

  // Determines if pasting should be allowed.
  void ShouldAllowPaste(base::OnceCallback<void(bool)> callback);

  // Determines if cutting should be allowed.
  void ShouldAllowCut(base::OnceCallback<void(bool)> callback);

  // Determines if sharing should be allowed.
  void ShouldAllowShare(base::OnceCallback<void(bool)> callback);

 private:
  friend class web::LazyWebStateUserData<DataControlsTabHelper>;
  explicit DataControlsTabHelper(web::WebState* web_state);
};

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_TAB_HELPER_H_
