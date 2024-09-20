// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/federated_identity_modal_dialog_view_delegate.h"
#include "content/public/browser/identity_request_account.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class WebContents;

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.webid
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: IdentityRequestDialogDisclosureField
enum class IdentityRequestDialogDisclosureField { kName, kEmail, kPicture };

struct CONTENT_EXPORT ClientMetadata {
  ClientMetadata(const GURL& terms_of_service_url,
                 const GURL& privacy_policy_url,
                 const GURL& brand_icon_url);
  ClientMetadata(const ClientMetadata& other);
  ~ClientMetadata();

  GURL terms_of_service_url;
  GURL privacy_policy_url;
  GURL brand_icon_url;
};

struct CONTENT_EXPORT IdentityCredentialTokenError {
  std::string code;
  GURL url;
};

struct CONTENT_EXPORT IdentityProviderMetadata {
  IdentityProviderMetadata();
  IdentityProviderMetadata(const IdentityProviderMetadata& other);
  ~IdentityProviderMetadata();

  std::optional<SkColor> brand_text_color;
  std::optional<SkColor> brand_background_color;
  GURL brand_icon_url;
  GURL idp_login_url;
  std::string requested_label;
  // For registered IdPs, the type is used to only show the accounts when the
  // RP is compatible.
  std::vector<std::string> types;
  // The URL of the configuration endpoint. This is stored in
  // IdentityProviderMetadata so that the UI code can pass it along when an
  // Account is selected by the user.
  GURL config_url;
  // Whether this IdP supports signing in to additional accounts.
  bool supports_add_account{false};
};

class CONTENT_EXPORT IdentityProviderData
    : public base::RefCounted<IdentityProviderData> {
 public:
  IdentityProviderData(const std::string& idp_for_display,
                       const IdentityProviderMetadata& idp_metadata,
                       const ClientMetadata& client_metadata,
                       blink::mojom::RpContext rp_context,
                       const std::vector<IdentityRequestDialogDisclosureField>&
                           disclosure_fields,
                       bool has_login_status_mismatch);

  std::string idp_for_display;
  IdentityProviderMetadata idp_metadata;
  ClientMetadata client_metadata;
  blink::mojom::RpContext rp_context;
  // For which fields should the dialog request permission for (assuming
  // this is for signup).
  std::vector<IdentityRequestDialogDisclosureField> disclosure_fields;
  // Whether there was some login status API mismatch when fetching the IDP's
  // accounts.
  bool has_login_status_mismatch;

 private:
  friend class base::RefCounted<IdentityProviderData>;

  ~IdentityProviderData();
};

// IdentityRequestDialogController is an interface, overridden and implemented
// by embedders, that controls the UI surfaces that are displayed to
// intermediate the exchange of federated accounts between identity providers
// and relying parties.
class CONTENT_EXPORT IdentityRequestDialogController {
 public:
  // This enum is used to back a histogram. Do not remove or reorder members.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.webid
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: IdentityRequestDialogDismissReason
  enum class DismissReason {
    kOther = 0,
    kCloseButton = 1,
    // Android-specific
    kSwipe = 2,
    // Android-specific
    kVirtualKeyboardShown = 3,
    kGotItButton = 4,
    kMoreDetailsButton = 5,
    // Android-specific
    kBackPress = 6,
    // Android-specific
    kTapScrim = 7,

    kMaxValue = kTapScrim,
  };

  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.webid
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: IdentityRequestDialogLinkType
  enum class LinkType { PRIVACY_POLICY, TERMS_OF_SERVICE };

  using AccountSelectionCallback =
      base::OnceCallback<void(const GURL& idp_config_url,
                              const std::string& /*account_id*/,
                              bool /*is_sign_in*/)>;
  using TokenCallback = base::OnceCallback<void(const std::string& /*token*/)>;

  using DismissCallback =
      base::OnceCallback<void(DismissReason dismiss_reason)>;
  using LoginToIdPCallback =
      base::RepeatingCallback<void(const GURL& /*idp_config_url*/,
                                   GURL /*idp_login_url*/)>;
  using MoreDetailsCallback = base::OnceCallback<void()>;
  using AccountsDisplayedCallback = base::OnceCallback<void()>;

  IdentityRequestDialogController() = default;

  IdentityRequestDialogController(const IdentityRequestDialogController&) =
      delete;
  IdentityRequestDialogController& operator=(
      const IdentityRequestDialogController&) = delete;

  virtual ~IdentityRequestDialogController() = default;

  // Returns the ideal size for the identity provider brand icon. The brand icon
  // is displayed in the accounts dialog.
  virtual int GetBrandIconIdealSize(blink::mojom::RpMode rp_mode);

  // Returns the minimum size for the identity provider brand icon. The brand
  // icon is displayed in the accounts dialog.
  virtual int GetBrandIconMinimumSize(blink::mojom::RpMode rp_mode);

  // When this is true, the dialog should not be immediately auto-accepted.
  virtual void SetIsInterceptionEnabled(bool enabled);

  // Shows and accounts selections for the given IDP. The `on_selected` callback
  // is called with the selected account id or empty string otherwise.
  // `sign_in_mode` represents whether this is an auto re-authn flow.
  // `new_accounts` are the accounts that were just logged in, which should
  // be prioritized in the UI. Returns true if the method successfully showed
  // UI. When false, the caller should assume that the API invocation was
  // terminated and the cleanup methods invoked.
  virtual bool ShowAccountsDialog(
      const std::string& rp_for_display,
      const std::vector<scoped_refptr<content::IdentityProviderData>>& idp_list,
      const std::vector<scoped_refptr<content::IdentityRequestAccount>>&
          accounts,
      content::IdentityRequestAccount::SignInMode sign_in_mode,
      blink::mojom::RpMode rp_mode,
      const std::vector<scoped_refptr<content::IdentityRequestAccount>>&
          new_accounts,
      AccountSelectionCallback on_selected,
      LoginToIdPCallback on_add_account,
      DismissCallback dismiss_callback,
      AccountsDisplayedCallback accounts_displayed_callback);

  // Shows a failure UI when the accounts fetch is failed such that it is
  // observable by users. This could happen when an IDP claims that the user is
  // signed in but not respond with any user account during browser fetches.
  // Returns true if the method successfully showed UI. When false, the caller
  // should assume that the API invocation was terminated and the cleanup
  // methods invoked.
  virtual bool ShowFailureDialog(const std::string& rp_for_display,
                                 const std::string& idp_for_display,
                                 blink::mojom::RpContext rp_context,
                                 blink::mojom::RpMode rp_mode,
                                 const IdentityProviderMetadata& idp_metadata,
                                 DismissCallback dismiss_callback,
                                 LoginToIdPCallback login_callback);

  // Shows an error UI when the user's sign-in attempt failed. Returns true if
  // the method successfully showed UI. When false, the caller should assume
  // that the API invocation was terminated and the cleanup methods invoked.
  virtual bool ShowErrorDialog(
      const std::string& rp_for_display,
      const std::string& idp_for_display,
      blink::mojom::RpContext rp_context,
      blink::mojom::RpMode rp_mode,
      const IdentityProviderMetadata& idp_metadata,
      const std::optional<IdentityCredentialTokenError>& error,
      DismissCallback dismiss_callback,
      MoreDetailsCallback more_details_callback);

  // Shows a loading UI when the user triggers a button flow and while waiting
  // for their accounts to be fetched. Returns true if the method successfully
  // showed UI. When false, the caller should assume that the API invocation was
  // terminated and the cleanup methods invoked.
  virtual bool ShowLoadingDialog(const std::string& rp_for_display,
                                 const std::string& idp_for_display,
                                 blink::mojom::RpContext rp_context,
                                 blink::mojom::RpMode rp_mode,
                                 DismissCallback dismiss_callback);

  // Only to be called after a dialog is shown.
  virtual std::string GetTitle() const;
  virtual std::optional<std::string> GetSubtitle() const;

  // Open a popup or similar that shows the specified URL.
  virtual void ShowUrl(LinkType type, const GURL& url);

  // Show a modal dialog that loads content from the IdP.
  virtual WebContents* ShowModalDialog(const GURL& url,
                                       blink::mojom::RpMode rp_mode,
                                       DismissCallback dismiss_callback);

  // Closes the modal dialog.
  virtual void CloseModalDialog();

  // When called on an object corresponding to the popup opened by
  // ShowModalDialog, returns the web contents for the original RP page.
  virtual WebContents* GetRpWebContents();

  // Request the user's permission to register an origin as an identity
  // provider. Calls the callback with a response of whether the request was
  // accepted or not.
  virtual void RequestIdPRegistrationPermision(
      const url::Origin& origin,
      base::OnceCallback<void(bool accepted)> callback);

 protected:
  bool is_interception_enabled_{false};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
