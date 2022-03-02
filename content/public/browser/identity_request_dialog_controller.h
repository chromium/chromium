// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
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

struct ClientIdData {
  ClientIdData(const GURL& tos_url, const GURL& privacy_policy_url);

  GURL terms_of_service_url;
  GURL privacy_policy_url;
};

struct CONTENT_EXPORT IdentityProviderMetadata {
  IdentityProviderMetadata();
  IdentityProviderMetadata(const IdentityProviderMetadata& other);
  ~IdentityProviderMetadata();

  absl::optional<SkColor> brand_text_color;
  absl::optional<SkColor> brand_background_color;
  SkBitmap brand_icon;
  GURL brand_icon_url;
};

// IdentityRequestDialogController is in interface for control of the UI
// surfaces that are displayed to intermediate the exchange of ID tokens.
class CONTENT_EXPORT IdentityRequestDialogController {
 public:
  enum class UserApproval {
    kApproved,
    kDenied,
  };

  enum class PermissionDialogMode {
    kStateless,
    kStateful,
  };

  using InitialApprovalCallback = base::OnceCallback<void(UserApproval)>;
  using IdProviderWindowClosedCallback = base::OnceCallback<void()>;
  using TokenExchangeApprovalCallback = base::OnceCallback<void(UserApproval)>;
  using AccountSelectionCallback =
      base::OnceCallback<void(const std::string&, bool)>;

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
      content::WebContents* rp_web_contents,
      const GURL& idp_signin_url,
      base::span<const IdentityRequestAccount> accounts,
      const IdentityProviderMetadata& idp_metadata,
      const ClientIdData& client_id_data,
      IdentityRequestAccount::SignInMode sign_in_mode,
      AccountSelectionCallback on_selected);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
