// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace content {
class WebContents;

extern const char CONTENT_EXPORT kSecFedCmCsrfHeader[];
extern const char CONTENT_EXPORT kSecFedCmCsrfHeaderValue[];

// Represents a federated user account which is used when displaying an account
// selector.
struct CONTENT_EXPORT IdentityRequestAccount {
  enum class LoginState {
    // This is a returning user signing in with RP/IDP in this browser.
    kSignIn,
    // This is a new user sign up for RP/IDP in *this browser*. Note that this
    // is the browser's notion of login state which may not match that of the
    // IDP. For example the user may actually be a returning user having
    // previously signed-up with this RP/IDP outside this browser. This is a
    // consequence of not relying the IDP's login state. This means that we
    // should be mindful to *NOT* rely on this value to mean definitely a new
    // user when using it to customize the UI.
    kSignUp,
  };

  enum class SignInMode {
    // This is the default sign in mode for returning users.
    kExplicit,
    // This represents the auto sign in flow. Currently it's only available when
    // RP specifies |preferAutoSignIn = true| AND there is only one signed in
    // account.
    kAuto,
  };

  IdentityRequestAccount(
      const std::string& id,
      const std::string& email,
      const std::string& name,
      const std::string& given_name,
      const GURL& picture,
      absl::optional<LoginState> login_state = absl::nullopt);
  IdentityRequestAccount(const IdentityRequestAccount&);
  ~IdentityRequestAccount();

  std::string id;
  std::string email;
  std::string name;
  std::string given_name;
  GURL picture;

  // The account login state. Unlike the other fields this one can be populated
  // either by the IDP or by the browser based on its stored permission grants.
  absl::optional<LoginState> login_state;
};

struct CONTENT_EXPORT ClientIdData {
  ClientIdData(const GURL& tos_url, const GURL& privacy_policy_url);
  ClientIdData(const ClientIdData& other);
  ~ClientIdData();

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
  // The URL of the configuration endpoint. This is stored in
  // IdentityProviderMetadata so that the UI code can pass it along when an
  // Account is selected by the user.
  GURL config_url;
};

struct CONTENT_EXPORT IdentityProviderData {
  IdentityProviderData(const std::string& idp_url_for_display,
                       const std::vector<IdentityRequestAccount>& accounts,
                       const IdentityProviderMetadata& idp_metadata,
                       const ClientIdData& client_id_data);
  IdentityProviderData(const IdentityProviderData& other);
  ~IdentityProviderData();

  std::string idp_for_display;
  std::vector<IdentityRequestAccount> accounts;
  IdentityProviderMetadata idp_metadata;
  ClientIdData client_id_data;
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
  // |sign_in_mode| represents whether this is an auto sign in flow.
  virtual void ShowAccountsDialog(
      WebContents* rp_web_contents,
      const std::string& rp_for_display,
      const std::vector<IdentityProviderData>& identity_provider_data,
      IdentityRequestAccount::SignInMode sign_in_mode,
      AccountSelectionCallback on_selected,
      DismissCallback dismiss_callback);

  // Shows a failure UI when the accounts fetch is failed such that it is
  // observable by users. This could happen when an IDP claims that the user is
  // signed in but not respond with any user account during browser fetches.
  virtual void ShowFailureDialog(WebContents* rp_web_contents,
                                 const std::string& rp_for_display,
                                 const std::string& idp_for_display,
                                 DismissCallback dismiss_callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
