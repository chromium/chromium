// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_TAB_HELPER_H_

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/enterprise/data_controls/core/browser/verdict.h"
#import "ios/chrome/browser/enterprise/data_controls/utils/clipboard_utils.h"
#import "ios/chrome/browser/enterprise/data_controls/utils/data_controls_utils.h"
#import "ios/chrome/browser/shared/public/commands/data_controls_commands.h"
#import "ios/web/public/lazy_web_state_user_data.h"
#import "url/gurl.h"

@protocol SnackbarCommands;

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
  bool ShouldAllowShare();

  // Sets the command handler for Data Controls.
  void SetDataControlsCommandsHandler(id<DataControlsCommands> handler);

  // Sets the snackbar handler.
  void SetSnackbarHandler(id<SnackbarCommands> snackbar_handler);

  // Called after the clipboard has been read from.
  void DidFinishClipboardRead();

 private:
  friend class web::LazyWebStateUserData<DataControlsTabHelper>;
  explicit DataControlsTabHelper(web::WebState* web_state);

  // Returns true if clipboard data controls are enabled.
  bool IsClipboardDataControlsEnabled() const;

  // Finalizes the copy action invoking the callback.
  void FinishCopy(const GURL& source_url,
                  base::WeakPtr<ProfileIOS> source_profile,
                  const ui::ClipboardMetadata& metadata,
                  CopyPolicyVerdicts verdicts,
                  base::OnceCallback<void(bool)> callback,
                  bool bypassed);

  // Finalizes the paste action invoking the callback.
  void FinishPaste(const GURL& destination_url,
                   const GURL& source_url,
                   base::WeakPtr<ProfileIOS> destination_profile,
                   base::WeakPtr<ProfileIOS> source_profile,
                   const ui::ClipboardMetadata& metadata,
                   Verdict verdict,
                   base::OnceCallback<void(bool)> callback,
                   bool bypassed);

  // Finalizes the share action invoking the callback.
  void FinishShare(const GURL& source_url,
                   Verdict verdict,
                   base::OnceCallback<void(bool)> callback,
                   bool bypassed);

  // Displays a warning dialog associated with a user's action (e.g., copy,
  // paste, share).
  void ShowWarningDialog(DataControlsDialog::Type dialog_type,
                         std::string_view org_domain,
                         base::OnceCallback<void(bool)> on_bypassed_callback);

  // Shows a snackbar to inform the user that an action was blocked by policy.
  void ShowRestrictSnackbar(std::string_view org_domain);

  // Returns the management domain for the given `profile`.
  std::string GetManagementDomain(ProfileIOS* profile);

  // Unowned pointer to the WebState owning `this`. `web_state_` will always
  // outlive `this`.
  raw_ptr<web::WebState> web_state_;

  // The data controller command handler.
  __weak id<DataControlsCommands> commands_handler_ = nil;

  // The snackbar command handler.
  __weak id<SnackbarCommands> snackbar_handler_ = nil;

  base::WeakPtrFactory<DataControlsTabHelper> weak_factory_{this};
};

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_TAB_HELPER_H_
