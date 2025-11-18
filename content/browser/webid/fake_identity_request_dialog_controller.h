// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_BROWSER_WEBID_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"

namespace base {
class Location;
}  // namespace base

namespace content {
class WebContents;

// This fakes the request dialogs to always provide user consent.
// Used by tests and if the --use-fake-ui-for-fedcm command-line
// flag is provided.
class CONTENT_EXPORT FakeIdentityRequestDialogController
    : public IdentityRequestDialogController,
      public WebContentsObserver {
 public:
  using IdentityProviderDataPtr = scoped_refptr<content::IdentityProviderData>;
  using IdentityRequestAccountPtr =
      scoped_refptr<content::IdentityRequestAccount>;
  using TokenError = content::IdentityCredentialTokenError;

  FakeIdentityRequestDialogController(
      std::optional<std::string> selected_account,
      WebContents* web_contents);
  ~FakeIdentityRequestDialogController() override;

  bool ShowAccountsDialog(
      content::RelyingPartyData rp_data,
      const std::vector<IdentityProviderDataPtr>& idp_list,
      const std::vector<IdentityRequestAccountPtr>& accounts,
      blink::mojom::RpMode rp_mode,
      AccountSelectionCallback on_selected,
      LoginToIdPCallback on_add_account,
      DismissCallback dismmiss_callback,
      AccountsDisplayedCallback accounts_displayed_callback) override;

  bool ShowFailureDialog(const RelyingPartyData& rp_data,
                         const std::string& idp_for_display,
                         blink::mojom::RpContext rp_context,
                         blink::mojom::RpMode rp_mode,
                         const IdentityProviderMetadata& idp_metadata,
                         DismissCallback dismiss_callback,
                         LoginToIdPCallback login_callback) override;

  bool ShowErrorDialog(const RelyingPartyData& rp_data,
                       const std::string& idp_for_display,
                       blink::mojom::RpContext rp_context,
                       blink::mojom::RpMode rp_mode,
                       const IdentityProviderMetadata& idp_metadata,
                       const std::optional<TokenError>& error,
                       DismissCallback dismiss_callback,
                       MoreDetailsCallback more_details_callback) override;

  bool ShowLoadingDialog(const RelyingPartyData& rp_data,
                         const std::string& idp_for_display,
                         blink::mojom::RpContext rp_context,
                         blink::mojom::RpMode rp_mode,
                         DismissCallback dismiss_callback) override;

  bool ShowVerifyingDialog(
      const content::RelyingPartyData& rp_data,
      const IdentityProviderDataPtr& idp_data,
      const IdentityRequestAccountPtr& account,
      content::IdentityRequestAccount::SignInMode sign_in_mode,
      blink::mojom::RpMode rp_mode,
      AccountsDisplayedCallback accounts_displayed_callback) override;

  std::string GetTitle() const override;
  std::optional<std::string> GetSubtitle() const override;

  void ShowUrl(LinkType link_type, const GURL& url) override;

  content::WebContents* ShowModalDialog(
      const GURL& url,
      blink::mojom::RpMode rp_mode,
      DismissCallback dismiss_callback) override;

  void CloseModalDialog() override;

  void WebContentsDestroyed() override;

  void RequestIdPRegistrationPermision(
      const url::Origin& origin,
      base::OnceCallback<void(bool accepted)> callback) override;

  bool DidShowUi() const override;

 private:
  void PostTask(const base::Location& from_here, base::OnceClosure task);

  std::optional<std::string> selected_account_;
  std::string title_;
  std::string subtitle_;
  // The caller ensures that this object does not outlive the web_contents_.
  raw_ptr<WebContents> web_contents_;
  // We observe WebContentsDestroyed to ensure that this pointer is valid.
  raw_ptr<WebContents> popup_window_{nullptr};
  DismissCallback popup_dismiss_callback_;
  bool did_show_ui_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
