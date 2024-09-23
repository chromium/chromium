// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_MODEL_LENS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_LENS_MODEL_LENS_TAB_HELPER_H_

#import <optional>

#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

enum class LensEntrypoint;
@protocol LensCommands;

// A tab helper that handles navigation to Lens action links, i.e.
// links that start with googlechromeaction://lens, by potentially
// opening the Lens UI.
class LensTabHelper : public web::WebStatePolicyDecider,
                      public web::WebStateUserData<LensTabHelper> {
 public:
  ~LensTabHelper() override;

  LensTabHelper(const LensTabHelper&) = delete;
  LensTabHelper& operator=(const LensTabHelper&) = delete;

  // Sets the Lens CommandDispatcher.
  void SetLensCommandsHandler(id<LensCommands> commands_handler);

  // Returns the entry point that the googlechromeaction:// url path points to.
  static std::optional<LensEntrypoint> EntryPointForGoogleChromeActionURLPath(
      NSString* path);

  // web::WebStatePolicyDecider:
  void ShouldAllowRequest(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;

 private:
  explicit LensTabHelper(web::WebState* web_state);

  // Opens the Lens input selection UI with the given entry point.
  void OpenLensInputSelection(LensEntrypoint entry_point);

  // Handler used to request showing the password bottom sheet.
  __weak id<LensCommands> commands_handler_;

  friend class web::WebStateUserData<LensTabHelper>;
  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_LENS_MODEL_LENS_TAB_HELPER_H_
