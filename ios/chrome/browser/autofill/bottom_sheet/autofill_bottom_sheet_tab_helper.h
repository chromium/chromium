// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_TAB_HELPER_H_

#import "components/autofill/core/common/unique_ids.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class ScriptMessage;
class WebFrame;
}  // namespace web

@class CommandDispatcher;
@protocol PasswordsAccountStorageNoticeHandler;
@protocol PasswordBottomSheetCommands;

class AutofillBottomSheetTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<AutofillBottomSheetTabHelper> {
 public:
  // Maximum number of times the password bottom sheet can be
  // dismissed before it gets disabled.
  static constexpr int kPasswordBottomSheetMaxDismissCount = 3;

  AutofillBottomSheetTabHelper(const AutofillBottomSheetTabHelper&) = delete;
  AutofillBottomSheetTabHelper& operator=(const AutofillBottomSheetTabHelper&) =
      delete;

  ~AutofillBottomSheetTabHelper() override;

  // Handler for JavaScript messages. Dispatch to more specific handler.
  void OnFormMessageReceived(const web::ScriptMessage& message);

  // Sets the Password CommandDispatcher.
  void SetPasswordBottomSheetHandler(
      id<PasswordBottomSheetCommands> password_bottom_sheet_commands_handler);

  // Prepare bottom sheet using data from the password form prediction.
  void AttachPasswordListeners(
      const std::vector<autofill::FieldRendererId>& renderer_ids,
      web::WebFrame* frame);

  // Detach the listeners, which will deactivate the bottom sheet.
  void DetachListenersAndRefocus(web::WebFrame* frame);

  // WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<AutofillBottomSheetTabHelper>;

  explicit AutofillBottomSheetTabHelper(
      web::WebState* web_state,
      id<PasswordsAccountStorageNoticeHandler>
          password_account_storage_notice_handler);

  // Check whether the password bottom sheet has been dismissed too many times
  // by the user.
  bool HasReachedDismissLimit();

  // Handler used to request showing the password bottom sheet.
  __weak id<PasswordBottomSheetCommands>
      password_bottom_sheet_commands_handler_;

  // Handler used for the passwords account storage notice.
  // TODO(crbug.com/1434606): Remove this when the move to account storage
  // notice is removed.
  __weak id<PasswordsAccountStorageNoticeHandler>
      password_account_storage_notice_handler_;

  // The WebState with which this object is associated.
  web::WebState* const web_state_;

  // List of password bottom sheet related renderer ids.
  std::set<autofill::FieldRendererId> registered_password_renderer_ids_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_TAB_HELPER_H_
