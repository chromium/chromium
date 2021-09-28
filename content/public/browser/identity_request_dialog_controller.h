// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {
class WebContents;

extern const char CONTENT_EXPORT kSecWebIdCsrfHeader[];

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

  IdentityRequestAccount(const std::string& sub,
                         const std::string& email,
                         const std::string& name,
                         const std::string& given_name,
                         const GURL& picture,
                         LoginState login_state = LoginState::kSignUp);
  IdentityRequestAccount(const IdentityRequestAccount&);
  ~IdentityRequestAccount();

  // sub, short for subject, is the unique identifier.
  std::string sub;
  std::string email;
  std::string name;
  std::string given_name;
  GURL picture;

  // The account login state. Unlike the other fields this one is not populated
  // by the IDP but instead by the browser based on its stored permission
  // grants.
  LoginState login_state;
};

struct ClientIdData {
  ClientIdData(const GURL& tos_url, const GURL& privacy_policy_url);

  GURL terms_of_service_url;
  GURL privacy_policy_url;
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

  using AccountList = std::vector<content::IdentityRequestAccount>;
  using InitialApprovalCallback = base::OnceCallback<void(UserApproval)>;
  using IdProviderWindowClosedCallback = base::OnceCallback<void()>;
  using TokenExchangeApprovalCallback = base::OnceCallback<void(UserApproval)>;
  using AccountSelectionCallback = base::OnceCallback<void(const std::string&)>;

  IdentityRequestDialogController() = default;

  IdentityRequestDialogController(const IdentityRequestDialogController&) =
      delete;
  IdentityRequestDialogController& operator=(
      const IdentityRequestDialogController&) = delete;

  virtual ~IdentityRequestDialogController() = default;

  // Permission-oriented flow methods.

  // Shows the initial permission dialog to the user.
  //
  // - |rp_web_contents| is the RP web contents that has initiated the
  //   identity request.
  // - |idp_url| is the IDP URL that gets displayed to the user.
  // - |mode| determines whether the dialog should maintain state across
  //   multiple calls
  // - |approval_callback| callback is called with appropriate status depending
  //   on whether user granted or denied the permission.
  //
  // 'IdentityRequestDialogController' is destroyed before
  // |rp_web_contents|.
  virtual void ShowInitialPermissionDialog(
      WebContents* rp_web_contents,
      const GURL& idp_url,
      PermissionDialogMode mode,
      InitialApprovalCallback approval_callback);

  // Shows and accounts selections for the given IDP. The |on_selected| callback
  // is called with the selected account id or empty string otherwise.
  // |sign_in_mode| represents whether this is an auto sign in flow.
  virtual void ShowAccountsDialog(
      content::WebContents* rp_web_contents,
      content::WebContents* idp_web_contents,
      const GURL& idp_signin_url,
      AccountList accounts,
      const ClientIdData& client_id_data,
      IdentityRequestAccount::SignInMode sign_in_mode,
      AccountSelectionCallback on_selected) {}

  // Shows the identity provider sign-in page at the given URL using the
  // |idp_web_contents| inside a modal window. The |on_closed| callback is
  // called when the window is closed by user or programmatically as a result of
  // invoking CloseIdProviderWindow().
  //
  // 'IdentityRequestDialogController' is destroyed before either WebContents.
  virtual void ShowIdProviderWindow(content::WebContents* rp_web_contents,
                                    content::WebContents* idp_web_contents,
                                    const GURL& idp_signin_url,
                                    IdProviderWindowClosedCallback on_closed);

  // Closes the identity provider sign-in window if any.
  virtual void CloseIdProviderWindow();

  // Shows the secondary permission dialog to the user.
  // - |rp_web_contents| is the RP web contents that has initiated the
  //   identity request.
  // - |idp_url| is the IDP URL that gets displayed to the user.
  // - |approval_callback| callback is called with appropriate status depending
  //   on whether user granted or denied the permission.
  virtual void ShowTokenExchangePermissionDialog(
      content::WebContents* rp_web_contents,
      const GURL& idp_url,
      TokenExchangeApprovalCallback approval_callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_DIALOG_CONTROLLER_H_
