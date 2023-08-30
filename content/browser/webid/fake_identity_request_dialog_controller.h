// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_BROWSER_WEBID_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include <string>

#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using TokenError = content::IdentityCredentialTokenError;

namespace content {
class WebContents;

// This fakes the request dialogs to always provide user consent.
// Used by tests and if the --use-fake-ui-for-fedcm command-line
// flag is provided.
class CONTENT_EXPORT FakeIdentityRequestDialogController
    : public IdentityRequestDialogController,
      public WebContentsObserver {
 public:
  explicit FakeIdentityRequestDialogController(
      absl::optional<std::string> selected_account,
      WebContents* web_contents = nullptr);
  ~FakeIdentityRequestDialogController() override;

  void ShowAccountsDialog(
      const std::string& top_frame_for_display,
      const absl::optional<std::string>& iframe_for_display,
      const std::vector<content::IdentityProviderData>& identity_provider_data,
      IdentityRequestAccount::SignInMode sign_in_mode,
      bool show_auto_reauthn_checkbox,
      AccountSelectionCallback on_selected,
      DismissCallback dismmiss_callback) override;

  void ShowFailureDialog(const std::string& top_frame_for_display,
                         const absl::optional<std::string>& iframe_for_display,
                         const std::string& idp_for_display,
                         const blink::mojom::RpContext& rp_context,
                         const IdentityProviderMetadata& idp_metadata,
                         DismissCallback dismiss_callback,
                         SigninToIdPCallback signin_callback) override;

  void ShowErrorDialog(const std::string& top_frame_for_display,
                       const absl::optional<std::string>& iframe_for_display,
                       const std::string& idp_for_display,
                       const blink::mojom::RpContext& rp_context,
                       const IdentityProviderMetadata& idp_metadata,
                       const absl::optional<TokenError>& error,
                       DismissCallback dismiss_callback) override;

  std::string GetTitle() const override;

  content::WebContents* ShowModalDialog(
      const GURL& url,
      DismissCallback dismiss_callback) override;

  void CloseModalDialog() override;

  void WebContentsDestroyed() override;

 private:
  absl::optional<std::string> selected_account_;
  std::string title_;
  // The caller ensures that this object does not outlive the web_contents_.
  raw_ptr<WebContents> web_contents_;
  // We observe WebContentsDestroyed to ensure that this pointer is valid.
  raw_ptr<WebContents> popup_window_{nullptr};
  DismissCallback popup_dismiss_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
