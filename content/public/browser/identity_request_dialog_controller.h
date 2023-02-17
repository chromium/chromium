// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/identity_request_account.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace content {
class WebContents;

struct CONTENT_EXPORT ClientMetadata {
  ClientMetadata(const GURL& tos_url, const GURL& privacy_policy_url);
  ClientMetadata(const ClientMetadata& other);
  ~ClientMetadata();

  GURL terms_of_service_url;
  GURL privacy_policy_url;
};

struct CONTENT_EXPORT IdentityProviderMetadata {
  IdentityProviderMetadata();
  IdentityProviderMetadata(const IdentityProviderMetadata& other);
  ~IdentityProviderMetadata();

  absl::optional<SkColor> brand_text_color;
  absl::optional<SkColor> brand_background_color;
  GURL brand_icon_url;
  GURL idp_signin_url;
  // The URL of the configuration endpoint. This is stored in
  // IdentityProviderMetadata so that the UI code can pass it along when an
  // Account is selected by the user.
  GURL config_url;
};

struct CONTENT_EXPORT IdentityProviderData {
  IdentityProviderData(const std::string& idp_url_for_display,
                       const std::vector<IdentityRequestAccount>& accounts,
                       const IdentityProviderMetadata& idp_metadata,
                       const ClientMetadata& client_metadata,
                       const blink::mojom::RpContext& rp_context);
  IdentityProviderData(const IdentityProviderData& other);
  ~IdentityProviderData();

  std::string idp_for_display;
  std::vector<IdentityRequestAccount> accounts;
  IdentityProviderMetadata idp_metadata;
  ClientMetadata client_metadata;
  blink::mojom::RpContext rp_context;
};

// IdentityRequestDialogController is in interface for control of the UI
// surfaces that are displayed to intermediate the exchange of ID tokens.
class CONTENT_EXPORT IdentityRequestDialogController {
 public:
  // This enum is used to back a histogram. Do not remove or reorder members.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.webid
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: IdentityRequestDialogDismissReason
  enum class DismissReason {
    OTHER = 0,
    CLOSE_BUTTON = 1,
    SWIPE = 2,
    VIRTUAL_KEYBOARD_SHOWN = 3,

    COUNT = 4,
  };

  using AccountSelectionCallback =
      base::OnceCallback<void(const GURL& idp_config_url,
                              const std::string& /*account_id*/,
                              bool /*is_sign_in*/)>;
  using DismissCallback =
      base::OnceCallback<void(DismissReason dismiss_reason)>;

  IdentityRequestDialogController() = default;

  IdentityRequestDialogController(const IdentityRequestDialogController&) =
      delete;
  IdentityRequestDialogController& operator=(
      const IdentityRequestDialogController&) = delete;

  virtual ~IdentityRequestDialogController() = default;

  // Returns the ideal size for the identity provider brand icon. The brand icon
  // is displayed in the accounts dialog.
  virtual int GetBrandIconIdealSize();

  // Returns the minimum size for the identity provider brand icon. The brand
  // icon is displayed in the accounts dialog.
  virtual int GetBrandIconMinimumSize();

  // Shows and accounts selections for the given IDP. The |on_selected| callback
  // is called with the selected account id or empty string otherwise.
  // |sign_in_mode| represents whether this is an auto re-authn flow.
  virtual void ShowAccountsDialog(
      WebContents* rp_web_contents,
      const std::string& rp_for_display,
      const std::vector<IdentityProviderData>& identity_provider_data,
      IdentityRequestAccount::SignInMode sign_in_mode,
      bool show_auto_reauthn_checkbox,
      AccountSelectionCallback on_selected,
      DismissCallback dismiss_callback);

  // Shows a failure UI when the accounts fetch is failed such that it is
  // observable by users. This could happen when an IDP claims that the user is
  // signed in but not respond with any user account during browser fetches.
  virtual void ShowFailureDialog(WebContents* rp_web_contents,
                                 const std::string& rp_for_display,
                                 const std::string& idp_for_display,
                                 DismissCallback dismiss_callback);

  // Show dialog notifying user that IdP sign-in failed.
  virtual void ShowIdpSigninFailureDialog(base::OnceClosure dismiss_callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
