// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_TAB_HELPER_H_

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/enterprise/data_controls/clipboard_utils.h"
#import "ios/web/public/lazy_web_state_user_data.h"
#import "url/gurl.h"

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

  void OnCopyAllowed(const GURL& source_url,
                     base::OnceCallback<void(bool)> callback,
                     CopyPolicyVerdicts copy_verdict);

  // Unowned pointer to the WebState owning `this`. `web_state_` will always
  // outlive `this`.
  raw_ptr<web::WebState> web_state_;

  base::WeakPtrFactory<DataControlsTabHelper> weak_factory_{this};
};

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_TAB_HELPER_H_
