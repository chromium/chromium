// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_BOTTOM_SHEET_BOTTOM_SHEET_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_BOTTOM_SHEET_BOTTOM_SHEET_TAB_HELPER_H_

#import "components/autofill/core/common/unique_ids.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class ScriptMessage;
}  // namespace web

@class CommandDispatcher;
@protocol PasswordBottomSheetCommands;

class BottomSheetTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<BottomSheetTabHelper> {
 public:
  BottomSheetTabHelper(const BottomSheetTabHelper&) = delete;
  BottomSheetTabHelper& operator=(const BottomSheetTabHelper&) = delete;

  ~BottomSheetTabHelper() override;

  // Handler for JavaScript messages. Dispatch to more specific handler.
  void OnFormMessageReceived(const web::ScriptMessage& message);

  // Sets the CommandDispatcher.
  void SetPasswordBottomSheetHandler(id<PasswordBottomSheetCommands> handler);

  // Prepare bottom sheet using data from the password form prediction.
  void AttachListeners(
      const std::vector<autofill::FieldRendererId>& renderer_ids,
      web::WebFrame* frame);

  // Detach the listeners, which will deactivate the bottom sheet.
  void DetachListenersAndRefocus(web::WebFrame* frame);

 private:
  friend class web::WebStateUserData<BottomSheetTabHelper>;

  explicit BottomSheetTabHelper(web::WebState* web_state);

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  id<PasswordBottomSheetCommands> handler_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_BOTTOM_SHEET_BOTTOM_SHEET_TAB_HELPER_H_
