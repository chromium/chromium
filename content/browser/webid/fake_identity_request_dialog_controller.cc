// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fake_identity_request_dialog_controller.h"

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

FakeIdentityRequestDialogController::FakeIdentityRequestDialogController(
    std::optional<std::string> selected_account,
    WebContents* web_contents)
    : selected_account_(selected_account), web_contents_(web_contents) {}

FakeIdentityRequestDialogController::~FakeIdentityRequestDialogController() =
    default;

bool FakeIdentityRequestDialogController::ShowAccountsDialog(
    content::RelyingPartyData rp_data,
    const std::vector<IdentityProviderDataPtr>& idp_list,
    const std::vector<IdentityRequestAccountPtr>& accounts,
    blink::mojom::RpMode rp_mode,
    AccountSelectionCallback on_selected,
    LoginToIdPCallback on_add_account,
    DismissCallback dismiss_callback,
    AccountsDisplayedCallback accounts_displayed_callback) {
  CHECK_GT(accounts.size(), 0ul);
  CHECK_GT(idp_list.size(), 0ul);

  // We're faking this so that browser automation and tests can verify that
  // the RP context was read properly.
  switch (idp_list[0]->rp_context) {
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
  if (!rp_data.iframe_for_display.empty()) {
    title_ += " to " + base::UTF16ToUTF8(rp_data.iframe_for_display);
    subtitle_ = "on " + base::UTF16ToUTF8(rp_data.rp_for_display);
  } else {
    title_ += " to " + base::UTF16ToUTF8(rp_data.rp_for_display);
  }

  // Use the provided account, if any. Otherwise do not run the callback right
  // away.
  if (selected_account_ && !is_interception_enabled_) {
    // TODO(crbug.com/364578201): This needs to be augmented to provide the
    // selected IDP. For now use the first one.
    PostTask(FROM_HERE, base::BindOnce(std::move(on_selected),
                                       idp_list[0]->idp_metadata.config_url,
                                       *selected_account_,
                                       /* is_sign_in= */ true));
  }
  did_show_ui_ = true;
  return true;
}

bool FakeIdentityRequestDialogController::ShowFailureDialog(
    const RelyingPartyData& rp_data,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const IdentityProviderMetadata& idp_metadata,
    DismissCallback dismiss_callback,
    LoginToIdPCallback login_callback) {
  title_ = "Confirm IDP Login";
  subtitle_ = "";
  did_show_ui_ = true;
  return true;
}

bool FakeIdentityRequestDialogController::ShowErrorDialog(
    const RelyingPartyData& rp_data,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error,
    DismissCallback dismiss_callback,
    MoreDetailsCallback more_details_callback) {
  if (!is_interception_enabled_) {
    DCHECK(dismiss_callback);
    // We don't need to call PostTask here because we're returning false.
    std::move(dismiss_callback).Run(DismissReason::kOther);
    return false;
  }
  did_show_ui_ = true;
  return true;
}

bool FakeIdentityRequestDialogController::ShowLoadingDialog(
    const RelyingPartyData& rp_data,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    DismissCallback dismiss_callback) {
  title_ = "Loading";
  subtitle_ = "";
  return true;
}

bool FakeIdentityRequestDialogController::ShowVerifyingDialog(
    const content::RelyingPartyData& rp_data,
    const IdentityProviderDataPtr& idp_data,
    const IdentityRequestAccountPtr& account,
    content::IdentityRequestAccount::SignInMode sign_in_mode,
    blink::mojom::RpMode rp_mode,
    AccountsDisplayedCallback accounts_displayed_callback) {
  title_ = sign_in_mode == content::IdentityRequestAccount::SignInMode::kAuto
               ? "Signing you in"
               : "Verifying";
  subtitle_ = "";
  did_show_ui_ = true;
  return true;
}

std::string FakeIdentityRequestDialogController::GetTitle() const {
  return title_;
}

std::optional<std::string> FakeIdentityRequestDialogController::GetSubtitle()
    const {
  if (subtitle_.empty()) {
    return std::nullopt;
  }
  return subtitle_;
}

void FakeIdentityRequestDialogController::ShowUrl(LinkType link_type,
                                                  const GURL& url) {
  if (!web_contents_) {
    return;
  }

  content::OpenURLParams params(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, /*is_renderer_initiated=*/false);
  web_contents_->GetDelegate()->OpenURLFromTab(
      web_contents_, params, /*navigation_handle_callback=*/{});
}

content::WebContents* FakeIdentityRequestDialogController::ShowModalDialog(
    const GURL& url,
    blink::mojom::RpMode rp_mode,
    DismissCallback dismiss_callback) {
  if (!web_contents_) {
    return nullptr;
  }

  popup_dismiss_callback_ = std::move(dismiss_callback);
  // This follows the code in FedCmModalDialogView::ShowPopupWindow.
  content::OpenURLParams params(
      url, content::Referrer(), WindowOpenDisposition::NEW_POPUP,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, /*is_renderer_initiated=*/false);
  popup_window_ = web_contents_->GetDelegate()->OpenURLFromTab(
      web_contents_, params, /*navigation_handle_callback=*/{});
  Observe(popup_window_);
  did_show_ui_ = true;
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

void FakeIdentityRequestDialogController::RequestIdPRegistrationPermision(
    const url::Origin& origin,
    base::OnceCallback<void(bool accepted)> callback) {
  if (!is_interception_enabled_) {
    PostTask(FROM_HERE, base::BindOnce(std::move(callback), false));
  }
}

bool FakeIdentityRequestDialogController::DidShowUi() const {
  return did_show_ui_;
}

void FakeIdentityRequestDialogController::PostTask(
    const base::Location& from_here,
    base::OnceClosure task) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(from_here,
                                                           std::move(task));
}

}  // namespace content
