// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_TAB_HELPER_H_

#import "base/scoped_multi_source_observation.h"
#import "components/autofill/core/browser/autofill_manager.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/common/unique_ids.h"
#include "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace autofill {
struct FormActivityParams;
}  // namespace autofill

namespace web {
class ScriptMessage;
}  // namespace web

@protocol AutofillBottomSheetCommands;
@class CommandDispatcher;
@protocol PasswordsAccountStorageNoticeHandler;

class AutofillBottomSheetTabHelper
    : public web::WebFramesManager::Observer,
      public web::WebStateObserver,
      public web::WebStateUserData<AutofillBottomSheetTabHelper>,
      public autofill::AutofillManager::Observer {
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

  // Sets the bottom sheet CommandDispatcher.
  void SetAutofillBottomSheetHandler(
      id<AutofillBottomSheetCommands> commands_handler);

  // Prepare bottom sheet using data from the password form prediction.
  void AttachPasswordListeners(
      const std::vector<autofill::FieldRendererId>& renderer_ids,
      const std::string& frame_id);

  // Detach the password listeners, which will deactivate the password bottom
  // sheet on the provided frame.
  void DetachPasswordListeners(const std::string& frame_id, bool refocus);

  // Detach the password listeners, which will deactivate the password bottom
  // sheet on all frames.
  void DetachPasswordListenersForAllFrames(bool refocus);

  // Detach the payments listeners, which will deactivate the payments bottom
  // sheet on the provided frame.
  void DetachPaymentsListeners(const std::string& frame_id, bool refocus);

  // Detach the payments listeners, which will deactivate the payments bottom
  // sheet on all frames.
  void DetachPaymentsListenersForAllFrames(bool refocus);

  // WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // web::WebFramesManager::Observer:
  void WebFrameBecameAvailable(web::WebFramesManager* web_frames_manager,
                               web::WebFrame* web_frame) override;

  // autofill::AutofillManager::Observer:
  void OnAutofillManagerDestroyed(autofill::AutofillManager& manager) override;
  void OnFieldTypesDetermined(autofill::AutofillManager& manager,
                              autofill::FormGlobalId form_id,
                              FieldTypeSource source) override;

 private:
  friend class web::WebStateUserData<AutofillBottomSheetTabHelper>;

  explicit AutofillBottomSheetTabHelper(
      web::WebState* web_state,
      id<PasswordsAccountStorageNoticeHandler>
          password_account_storage_notice_handler);

  // Check whether the password bottom sheet has been dismissed too many times
  // by the user.
  bool HasReachedDismissLimit();

  // Prepare bottom sheet using data from the form prediction.
  void AttachListeners(
      const std::vector<autofill::FieldRendererId>& renderer_ids,
      std::set<autofill::FieldRendererId>& registered_renderer_ids,
      const std::string& frame_id,
      bool must_be_empty);

  // Detach listeners, which will deactivate the associated bottom sheet.
  void DetachListenersForAllFrames(
      const std::set<autofill::FieldRendererId>& renderer_ids,
      bool must_be_empty,
      bool refocus);

  // Send command to show the Password Bottom Sheet.
  void ShowPasswordBottomSheet(const autofill::FormActivityParams params);

  // Send command to show the Payments Bottom Sheet.
  void ShowPaymentsBottomSheet(const autofill::FormActivityParams params);

  // Handler used to request showing the password bottom sheet.
  __weak id<AutofillBottomSheetCommands> commands_handler_;

  // Handler used for the passwords account storage notice.
  // TODO(crbug.com/1434606): Remove this when the move to account storage
  // notice is removed.
  __weak id<PasswordsAccountStorageNoticeHandler>
      password_account_storage_notice_handler_;

  // The WebState with which this object is associated.
  web::WebState* const web_state_;

  // TODO(crbug.com/1441921): Remove once this class uses FormGlobalIds.
  base::ScopedObservation<web::WebFramesManager,
                          web::WebFramesManager::Observer>
      frames_manager_observation_{this};

  base::ScopedMultiSourceObservation<autofill::AutofillManager,
                                     autofill::AutofillManager::Observer>
      autofill_manager_observations_{this};

  // List of password bottom sheet related renderer ids.
  // TODO(crbug.com/1441921): Maybe migrate to FieldGlobalIds.
  std::set<autofill::FieldRendererId> registered_password_renderer_ids_;

  // List of payments bottom sheet related renderer ids.
  // TODO(crbug.com/1441921): Migrate to FieldGlobalIds.
  std::set<autofill::FieldRendererId> registered_payments_renderer_ids_;

  // List of frames on which listeners have been attached.
  std::set<std::string> web_frames_ids_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_TAB_HELPER_H_
