// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_TAB_HELPER_H_

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "components/enterprise/common/proto/connectors.pb.h"
#import "components/enterprise/data_controls/core/browser/verdict.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_pasteboard_manager.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_pasteboard_manager_observer.h"
#import "ios/chrome/browser/enterprise/data_controls/utils/clipboard_utils.h"
#import "ios/chrome/browser/enterprise/enterprise_dialog/model/warning_dialog.h"
#import "ios/chrome/browser/shared/public/commands/enterprise_commands.h"
#import "ios/web/public/web_state_user_data.h"
#import "url/gurl.h"

@protocol SnackbarCommands;

namespace enterprise_connectors {
struct RequestHandlerResult;
}

namespace web {
class WebState;
}

namespace data_controls {

class DataControlsPasteboardManager;

// Manages Enterprise Data Control policies for the associated tab. These
// policies determine whether certain user actions, like clipboard operations
// (copying, pasting), are permitted. Such restrictions only apply to managed
// profiles; for all other profiles, these actions are unrestricted.
class DataControlsTabHelper
    : public web::WebStateUserData<DataControlsTabHelper>,
      public DataControlsPasteboardManagerObserver {
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
  bool ShouldAllowShare();

  // Returns true if the Search With data controls feature is enabled.
  static bool IsSearchWithFeatureEnabled();

  // Determines if the "Search With [Default Search Engine]" action is allowed
  // for the current tab (source URL) by enterprise policies. This is a
  // synchronous check used during context menu construction to decide if the
  // context menu should include this item.
  bool IsSearchWithAllowed();

  // Determines if the "Search With [Default Search Engine]" action should be
  // executed for the current tab (source URL) by enterprise policies. Checks
  // the policy's verdict and manages the action: allows, reports, or shows a
  // warning dialog and executes the search asynchronously only if the user
  // explicitly proceeds.
  void ShouldAllowSearchWith(size_t text_length,
                             base::OnceCallback<void(bool)> callback);

  // Sets the command handler for Enterprise.
  void SetEnterpriseCommandsHandler(id<EnterpriseCommands> handler);

  // Sets the snackbar handler.
  void SetSnackbarHandler(id<SnackbarCommands> snackbar_handler);

  // Called after the clipboard has been read from.
  void DidFinishClipboardRead();

  // DataControlsPasteboardManagerObserver override: Called when the pasteboard
  // content is changed.
  void OnPasteboardContentChanged() override;

 private:
  friend class web::WebStateUserData<DataControlsTabHelper>;
  explicit DataControlsTabHelper(web::WebState* web_state);

  // An enum class that keeps track of the state of the current paste event for
  // each tab. More event states will be added in the future for the Pasted
  // Content DLP Rules feature.
  //
  // TODO(crbug.com/531672160): Add event states for pasted content DLP Rules.
  enum class PasteEventState {
    // No ongoing paste event.
    kIdle,
    // Waiting for users to make a decision from Warning Dialog.
    kDisplayingWarningDialog,
  };

  // Returns true if clipboard data controls are enabled.
  bool IsClipboardDataControlsEnabled() const;

  // Block the paste if the action outcome of Data Controls is Block, or the
  // user decides to cancel on Warn. Otherwise, start a Pasted Content Analysis
  // if it is enabled for either the source profile or destination profile.
  void PasteIfAllowedByDataControls(
      const GURL& destination_url,
      const GURL& source_url,
      base::WeakPtr<ProfileIOS> destination_profile,
      base::WeakPtr<ProfileIOS> source_profile,
      const ui::ClipboardMetadata& metadata,
      Verdict verdict,
      base::OnceCallback<void(bool)> callback,
      bool bypassed);

  // Allow or block the paste or show a warning dialog based on the `result`.
  void PasteIfAllowedByContentAnalysis(
      base::OnceCallback<void(bool)> callback,
      enterprise_connectors::RequestHandlerResult result);

  // Run the pasted content analysis using the `PastedContentHandlerIOS` with
  // the info from the parameters.
  void RunPastedContentAnalysis(
      const GURL& destination_url,
      base::WeakPtr<ProfileIOS> profile,
      enterprise_connectors::ContentMetaData::CopiedTextSource copied_source,
      base::OnceCallback<void(bool)> callback,
      std::optional<PasteboardContentDLP> pasteboard_content);

  // Finalizes the copy action invoking the callback.
  void FinishCopy(const GURL& source_url,
                  base::WeakPtr<ProfileIOS> source_profile,
                  const ui::ClipboardMetadata& metadata,
                  CopyPolicyVerdicts verdicts,
                  base::OnceCallback<void(bool)> callback,
                  bool bypassed);

  // Restores the pasteboard item to pasteboard if needed and then finalizes the
  // paste action invoking the callback and resets `paste_event_state_` to
  // `kIdle`.
  void FinishPaste(base::OnceCallback<void(bool)> callback,
                   bool verdict_or_scan_success,
                   bool analysis_warn_bypassed);

  // Finalizes the share action invoking the callback.
  void FinishShare(const GURL& source_url,
                   Verdict verdict,
                   base::OnceCallback<void(bool)> callback,
                   bool bypassed);

  // Finalizes the search with action invoking the callback.
  void FinishSearchWith(const GURL& source_url,
                        base::WeakPtr<ProfileIOS> source_profile,
                        const ui::ClipboardMetadata& metadata,
                        Verdict verdict,
                        base::OnceCallback<void(bool)> callback,
                        bool bypassed);

  // Displays a warning dialog associated with a user's action (e.g., copy,
  // paste, share).
  void ShowWarningDialog(enterprise::DialogType dialog_type,
                         std::string_view org_domain,
                         base::OnceCallback<void(bool)> on_bypassed_callback);

  // Shows a snackbar to inform the user that an action was blocked by policy.
  void ShowRestrictSnackbar(std::string_view org_domain);

  // Returns the management domain for the given `profile`.
  std::string GetManagementDomain(ProfileIOS* profile);

  // Unowned pointer to the WebState owning `this`. `web_state_` will always
  // outlive `this`.
  raw_ptr<web::WebState> web_state_;

  // The enterprise command handler.
  __weak id<EnterpriseCommands> enterprise_handler_ = nil;

  // The snackbar command handler.
  __weak id<SnackbarCommands> snackbar_handler_ = nil;

  PasteEventState paste_event_state_ = PasteEventState::kIdle;

  base::ScopedObservation<DataControlsPasteboardManager,
                          DataControlsPasteboardManagerObserver>
      scoped_observation_{this};
  base::WeakPtrFactory<DataControlsTabHelper> weak_factory_{this};
};

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_TAB_HELPER_H_
