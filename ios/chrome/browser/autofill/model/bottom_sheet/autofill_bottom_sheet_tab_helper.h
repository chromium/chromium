// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "components/autofill/core/browser/autofill_manager.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/password_manager/ios/password_generation_provider.h"
#import "components/plus_addresses/plus_address_types.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/virtual_card_enrollment_callbacks.h"
#include "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "url/origin.h"

namespace autofill {
class AutofillBottomSheetObserver;
class CardUnmaskAuthenticationSelectionDialogControllerImpl;
struct FormActivityParams;
class VirtualCardEnrollUiModel;
}  // namespace autofill

namespace web {
class ScriptMessage;
}  // namespace web

@protocol AutofillCommands;
@class CommandDispatcher;

// This class manages state and events relating to the showing of various bottom
// sheets for Autofill/Password Manager.
//
// Some bottom sheets show in response to browser-layer interactions. These can
// be instantiated directly using public Show... methods.
//
// Others show in response to JS-layer interactions. In these cases, this class
// attaches/detaches listeners in the document, and shows the appropriate bottom
// sheet when these listeners are triggered.
class AutofillBottomSheetTabHelper
    : public web::WebFramesManager::Observer,
      public web::WebStateObserver,
      public web::WebStateUserData<AutofillBottomSheetTabHelper>,
      public autofill::AutofillManager::Observer {
 public:
  // Maximum number of times the password bottom sheet can be
  // dismissed before it gets disabled.
  static constexpr int kPasswordBottomSheetMaxDismissCount = 3;

  // Maximum number of times the password generation bottom sheet can be
  // dismissed before it gets disabled.
  static constexpr int kPasswordGenerationBottomSheetMaxDismissCount = 3;

  AutofillBottomSheetTabHelper(const AutofillBottomSheetTabHelper&) = delete;
  AutofillBottomSheetTabHelper& operator=(const AutofillBottomSheetTabHelper&) =
      delete;

  ~AutofillBottomSheetTabHelper() override;

  // Observer registration methods.
  void AddObserver(autofill::AutofillBottomSheetObserver* observer);
  void RemoveObserver(autofill::AutofillBottomSheetObserver* observer);

  // Shows the card unmask authentication selection bottom sheet. The
  // `model_controller` are stored and retrieved later by the appropriate
  // coordinator.
  void ShowCardUnmaskAuthenticationSelection(
      std::unique_ptr<
          autofill::CardUnmaskAuthenticationSelectionDialogControllerImpl>
          model_controller);

  // Shows the plus address bottom sheet, taken in response to choosing a
  // `kCreateNewPlusAddress` autofill suggestion. Also stores `callback` for
  // if/when the UI completes successfully.
  void ShowPlusAddressesBottomSheet(
      plus_addresses::PlusAddressCallback callback);

  // Send a command to show the VCN enrollment Bottom Sheet.
  void ShowVirtualCardEnrollmentBottomSheet(
      std::unique_ptr<autofill::VirtualCardEnrollUiModel> model,
      autofill::VirtualCardEnrollmentCallbacks callbacks);

  // Send a command to show the bottom sheet to edit an address.
  // The address to be shown/worked upon in this bottom sheet is fetched via the
  // `AutofillSaveUpdateAddressProfileDelegateIOS` delegate.
  void ShowEditAddressBottomSheet();

  // Handler for JavaScript messages. Dispatch to more specific handler.
  void OnFormMessageReceived(const web::ScriptMessage& message);

  // Sets the bottom sheet CommandDispatcher.
  void SetAutofillBottomSheetHandler(id<AutofillCommands> commands_handler);

  // Sets the password generation provider used for proactive password
  // generation.
  void SetPasswordGenerationProvider(
      id<PasswordGenerationProvider> generation_provider);

  // Prepare bottom sheet using data from the password form prediction.
  void AttachPasswordListeners(
      const std::vector<autofill::FieldRendererId>& renderer_ids,
      const std::string& frame_id);

  // Prepare proactive password generation bottom sheet using data from the
  // password form prediction.
  void AttachPasswordGenerationListeners(
      const std::vector<autofill::FieldRendererId>& renderer_ids,
      const std::string& frame_id);

  // Detach the password listeners, which will deactivate the password bottom
  // sheet on the provided frame.
  void DetachPasswordListeners(const std::string& frame_id, bool refocus);

  // Detach the password listeners, which will deactivate the password bottom
  // sheet on all frames.
  void DetachPasswordListenersForAllFrames();

  // Detach the password generation listeners, which will deactivate the
  // proactive password generation bottom sheet on all frames.
  void DetachPasswordGenerationListenersForAllFrames();

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
  void OnAutofillManagerStateChanged(
      autofill::AutofillManager& manager,
      autofill::AutofillManager::LifecycleState old_state,
      autofill::AutofillManager::LifecycleState new_state) override;
  void OnFieldTypesDetermined(autofill::AutofillManager& manager,
                              autofill::FormGlobalId form_id,
                              FieldTypeSource source) override;

  // Returns the controller for authentication selection.
  // The caller takes ownership and subsequent calls will return nullptr until
  // another instance of the dialog is shown again by calling
  // ShowCardUnmaskAuthenticationSelection().
  std::unique_ptr<
      autofill::CardUnmaskAuthenticationSelectionDialogControllerImpl>
  GetCardUnmaskAuthenticationSelectionDialogController();

  // Used to get the callback to be run on completion of the plus_address UI.
  plus_addresses::PlusAddressCallback GetPendingPlusAddressFillCallback();

  // Used to get the callbacks to be run on completion of the VCN enrollment UI.
  // This value is moved and should only be retrieved once per bottom sheet.
  autofill::VirtualCardEnrollmentCallbacks GetVirtualCardEnrollmentCallbacks();

 private:
  friend class web::WebStateUserData<AutofillBottomSheetTabHelper>;

  explicit AutofillBottomSheetTabHelper(web::WebState* web_state);

  // Check whether the password bottom sheet has been dismissed too many times
  // by the user.
  bool HasReachedPasswordSuggestionDismissLimit();

  // Check whether the password generation bottom sheet has been dismissed
  // too many times by the user.
  bool HasReachedPasswordGenerationDismissLimit();

  // Prepare bottom sheet using data from the form prediction.
  void AttachListeners(
      const std::vector<autofill::FieldRendererId>& renderer_ids,
      std::set<autofill::FieldRendererId>& registered_renderer_ids,
      const std::string& frame_id,
      bool allow_autofocus);

  // Detach listeners, which will deactivate the associated bottom sheet.
  void DetachListenersForFrame(
      const std::string& frame_id,
      const std::set<autofill::FieldRendererId>& renderer_ids,
      bool refocus);

  // Send command to show the Password Bottom Sheet.
  void ShowPasswordBottomSheet(const autofill::FormActivityParams params);

  // Send command to show the Payments Bottom Sheet.
  void ShowPaymentsBottomSheet(const autofill::FormActivityParams params);

  // Shows the password generation suggestion view controller.
  void ShowProactivePasswordGenerationBottomSheet(
      const autofill::FormActivityParams& params);

  // Password generation provider used to trigger proactive password generation
  id<PasswordGenerationProvider> generation_provider_;

  // Handler used to request showing the password bottom sheet.
  __weak id<AutofillCommands> commands_handler_;

  // The WebState with which this object is associated.
  const raw_ptr<web::WebState> web_state_;

  // TODO(crbug.com/40266699): Remove once this class uses FormGlobalIds.
  base::ScopedObservation<web::WebFramesManager,
                          web::WebFramesManager::Observer>
      frames_manager_observation_{this};

  base::ScopedMultiSourceObservation<autofill::AutofillManager,
                                     autofill::AutofillManager::Observer>
      autofill_manager_observations_{this};

  // List of password bottom sheet related renderer ids, mapped to a frame id.
  // TODO(crbug.com/40266699): Maybe migrate to FieldGlobalIds.
  std::map<std::string, std::set<autofill::FieldRendererId>>
      registered_password_renderer_ids_;

  // List of payments bottom sheet related renderer ids, mapped to a frame id.
  // TODO(crbug.com/40266699): Migrate to FieldGlobalIds.
  std::map<std::string, std::set<autofill::FieldRendererId>>
      registered_payments_renderer_ids_;

  // Set of proactive password generation bottom sheet related renderer ids,
  // mapped to their frame id.
  // TODO(crbug.com/40266699): Migrate to FieldGlobalIds.
  std::map<std::string, std::set<autofill::FieldRendererId>>
      registered_password_generation_renderer_ids_;

  base::ObserverList<autofill::AutofillBottomSheetObserver>::Unchecked
      observers_;

  // A controller for the authentication selection. This will be reset once
  // GetCardUnmaskAuthenticationSelectionDialogController is called.
  std::unique_ptr<
      autofill::CardUnmaskAuthenticationSelectionDialogControllerImpl>
      card_unmask_authentication_selection_controller_;

  // A callback to be run on completion of the plus address bottom sheet UI
  // flow.
  plus_addresses::PlusAddressCallback pending_plus_address_callback_;

  // Callbacks to be run when the virtual card enrollment bottom sheet UI has
  // completed.
  autofill::VirtualCardEnrollmentCallbacks virtual_card_enrollment_callbacks_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_BOTTOM_SHEET_AUTOFILL_BOTTOM_SHEET_TAB_HELPER_H_
