// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fake_identity_request_dialog_controller.h"

#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

FakeIdentityRequestDialogController::FakeIdentityRequestDialogController(
    absl::optional<std::string> selected_account,
    WebContents* web_contents)
    : selected_account_(selected_account), web_contents_(web_contents) {}

FakeIdentityRequestDialogController::~FakeIdentityRequestDialogController() =
    default;

void FakeIdentityRequestDialogController::ShowAccountsDialog(
    const std::string& top_frame_for_display,
    const absl::optional<std::string>& iframe_for_display,
    const std::vector<content::IdentityProviderData>& identity_provider_data,
    IdentityRequestAccount::SignInMode sign_in_mode,
    bool show_auto_reauthn_checkbox,
    AccountSelectionCallback on_selected,
    DismissCallback dismiss_callback) {
  // TODO(crbug.com/1348262): Temporarily support only the first IDP, extend to
  // support multiple IDPs.
  std::vector<IdentityRequestAccount> accounts =
      identity_provider_data[0].accounts;
  CHECK_GT(accounts.size(), 0ul);
  CHECK_GT(identity_provider_data.size(), 0ul);

  // We're faking this so that browser automation and tests can verify that
  // the RP context was read properly.
  switch (identity_provider_data[0].rp_context) {
    case blink::mojom::RpContext::kSignIn:
      title_ = "Sign in";
      break;
    case blink::mojom::RpContext::kSignUp:
      title_ = "Sign up";
      break;
    case blink::mojom::RpContext::kUse:
      title_ = "Use";
      break;
    case blink::mojom::RpContext::kContinue:
      title_ = "Continue";
      break;
  };

  if (is_interception_enabled_) {
    // Browser automation will handle selecting an account/canceling.
    return;
  }
  // Use the provided account, if any. Otherwise do not run the callback right
  // away.
  if (selected_account_) {
    std::move(on_selected)
        .Run(identity_provider_data[0].idp_metadata.config_url,
             *selected_account_,
             /* is_sign_in= */ true);
  } else if (sign_in_mode == IdentityRequestAccount::SignInMode::kAuto) {
    std::move(on_selected)
        .Run(identity_provider_data[0].idp_metadata.config_url,
             identity_provider_data[0].accounts[0].id, /* is_sign_in= */ true);
  }
}

void FakeIdentityRequestDialogController::ShowFailureDialog(
    const std::string& top_frame_for_display,
    const absl::optional<std::string>& iframe_for_display,
    const std::string& idp_for_display,
    const blink::mojom::RpContext& rp_context,
    const IdentityProviderMetadata& idp_metadata,
    DismissCallback dismiss_callback,
    SigninToIdPCallback signin_callback) {
  title_ = "Confirm IDP Login";
}

void FakeIdentityRequestDialogController::ShowErrorDialog(
    const std::string& top_frame_for_display,
    const absl::optional<std::string>& iframe_for_display,
    const std::string& idp_for_display,
    const blink::mojom::RpContext& rp_context,
    const IdentityProviderMetadata& idp_metadata,
    const absl::optional<TokenError>& error,
    DismissCallback dismiss_callback,
    MoreDetailsCallback more_details_callback) {
  DCHECK(dismiss_callback);
  std::move(dismiss_callback).Run(DismissReason::kOther);
}

std::string FakeIdentityRequestDialogController::GetTitle() const {
  return title_;
}

content::WebContents* FakeIdentityRequestDialogController::ShowModalDialog(
    const GURL& url,
    DismissCallback dismiss_callback) {
  if (!web_contents_) {
    return nullptr;
  }

  popup_dismiss_callback_ = std::move(dismiss_callback);
  // This follows the code in FedCmModalDialogView::ShowPopupWindow.
  content::OpenURLParams params(
      url, content::Referrer(), WindowOpenDisposition::NEW_POPUP,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, /*is_renderer_initiated=*/false);
  popup_window_ =
      web_contents_->GetDelegate()->OpenURLFromTab(web_contents_, params);
  Observe(popup_window_);
  return popup_window_;
}

void FakeIdentityRequestDialogController::CloseModalDialog() {
  // We do not want to trigger the dismiss callback when we close the popup
  // here, because that would abort the signin flow.
  popup_dismiss_callback_.Reset();
  if (popup_window_) {
    // Store this in a local variable to avoid triggering the dangling pointer
    // detector.
    WebContents* web_contents = popup_window_;
    popup_window_ = nullptr;
    web_contents->Close();
  }
}

void FakeIdentityRequestDialogController::WebContentsDestroyed() {
  if (popup_dismiss_callback_) {
    std::move(popup_dismiss_callback_).Run(DismissReason::kOther);
  }
  popup_window_ = nullptr;
}

}  // namespace content
