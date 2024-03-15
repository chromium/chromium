// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_BROWSER_WEBID_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include <optional>
#include <string>

#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents_observer.h"

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
      std::optional<std::string> selected_account,
      WebContents* web_contents = nullptr);
  ~FakeIdentityRequestDialogController() override;

  void ShowAccountsDialog(
      const std::string& top_frame_for_display,
      const std::optional<std::string>& iframe_for_display,
      const std::vector<content::IdentityProviderData>& identity_provider_data,
      IdentityRequestAccount::SignInMode sign_in_mode,
      blink::mojom::RpMode rp_mode,
      const std::optional<content::IdentityProviderData>& new_account_idp,
      AccountSelectionCallback on_selected,
      LoginToIdPCallback on_add_account,
      DismissCallback dismmiss_callback,
      AccountsDisplayedCallback accounts_displayed_callback) override;

  void ShowFailureDialog(const std::string& top_frame_for_display,
                         const std::optional<std::string>& iframe_for_display,
                         const std::string& idp_for_display,
                         blink::mojom::RpContext rp_context,
                         blink::mojom::RpMode rp_mode,
                         const IdentityProviderMetadata& idp_metadata,
                         DismissCallback dismiss_callback,
                         LoginToIdPCallback login_callback) override;

  void ShowErrorDialog(const std::string& top_frame_for_display,
                       const std::optional<std::string>& iframe_for_display,
                       const std::string& idp_for_display,
                       blink::mojom::RpContext rp_context,
                       blink::mojom::RpMode rp_mode,
                       const IdentityProviderMetadata& idp_metadata,
                       const std::optional<TokenError>& error,
                       DismissCallback dismiss_callback,
                       MoreDetailsCallback more_details_callback) override;

  void ShowLoadingDialog(const std::string& top_frame_for_display,
                         const std::string& idp_for_display,
                         blink::mojom::RpContext rp_context,
                         blink::mojom::RpMode rp_mode,
                         DismissCallback dismiss_callback) override;

  std::string GetTitle() const override;

  void ShowUrl(LinkType link_type, const GURL& url) override;

  content::WebContents* ShowModalDialog(
      const GURL& url,
      DismissCallback dismiss_callback) override;

  void CloseModalDialog() override;

  void WebContentsDestroyed() override;

  void RequestIdPRegistrationPermision(
      const url::Origin& origin,
      base::OnceCallback<void(bool accepted)> callback) override;

 private:
  std::optional<std::string> selected_account_;
  std::string title_;
  // The caller ensures that this object does not outlive the web_contents_.
  raw_ptr<WebContents> web_contents_;
  // We observe WebContentsDestroyed to ensure that this pointer is valid.
  raw_ptr<WebContents> popup_window_{nullptr};
  DismissCallback popup_dismiss_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FAKE_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
