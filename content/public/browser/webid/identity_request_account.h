// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEBID_IDENTITY_REQUEST_ACCOUNT_H_
#define CONTENT_PUBLIC_BROWSER_WEBID_IDENTITY_REQUEST_ACCOUNT_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/webid/login_status_account.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace content {

class IdentityProviderData;

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.webid
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: IdentityRequestDialogDisclosureField
enum class IdentityRequestDialogDisclosureField : int32_t {
  kName,
  kEmail,
  kPicture,
  kPhoneNumber,
  kUsername
};

// Represents a federated user account which is used when displaying the FedCM
// account selector.
class CONTENT_EXPORT IdentityRequestAccount
    : public base::RefCounted<IdentityRequestAccount> {
 public:
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
    // This represents the auto re-authn flow. Currently it's only available
    // when RP specifies |autoReauthn = true| AND there is only one signed in
    // account.
    kAuto,
  };

  enum class DisplayPriority {
    // An account that was logged into before the request.
    kRegular,
    // An account that was newly logged into during the request.
    kNew,
  };

  IdentityRequestAccount(
      const std::string& id,
      const std::string& display_identifier,
      const std::string& display_name,
      const std::string& email,
      const std::string& name,
      const std::string& given_name,
      const GURL& picture,
      const std::string& phone,
      const std::string& username,
      std::vector<std::string> login_hints,
      std::vector<std::string> domain_hints,
      std::vector<std::string> labels,
      std::optional<LoginState> idp_claimed_login_state = std::nullopt,
      LoginState browser_trusted_login_state = LoginState::kSignUp,
      std::optional<base::Time> last_used_timestamp = std::nullopt);

  explicit IdentityRequestAccount(
      const blink::common::webid::LoginStatusAccount& account);

  // The identity provider to which the account belongs to. This is not set in
  // the constructor but instead set later.
  scoped_refptr<IdentityProviderData> identity_provider = nullptr;

  std::string id;
  // E.g. email or phone number
  std::string display_identifier;
  // E.g. the user's full name or username
  std::string display_name;
  std::string email;
  std::string name;
  std::string given_name;
  GURL picture;
  std::string phone;
  std::string username;
  // This will be an empty image if fetching failed.
  gfx::Image decoded_picture;

  std::vector<std::string> login_hints;
  std::vector<std::string> domain_hints;
  std::vector<std::string> labels;

  // The list of fields the UI should prompt the user for. This is based on the
  // fields that the RP requested and affected by the login state and the
  // actual available fields in the IDP accounts response.
  std::vector<IdentityRequestDialogDisclosureField> fields;

  // The account login state populated by the IDP through an approved clients
  // list.
  std::optional<LoginState> idp_claimed_login_state;

  // The account login state populated by the browser based on stored permission
  // grants.
  LoginState browser_trusted_login_state;

  // The last used timestamp, or nullopt if the account has not been used
  // before.
  std::optional<base::Time> last_used_timestamp;

  // How the account should be prioritized in the UI.
  DisplayPriority display_priority = DisplayPriority::kRegular;

  // Whether this account is filtered out or not. An account may be filtered out
  // due to login hint, domain hint, or account label.
  bool is_filtered_out = false;

  // Whether this account was retrieved from the Lightweight FedCM Accounts Push
  // storage. If this is true, the request for the account picture will only
  // check against cache, and will fail on cache miss.
  bool from_accounts_push = false;

 private:
  friend class base::RefCounted<IdentityRequestAccount>;

  ~IdentityRequestAccount();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEBID_IDENTITY_REQUEST_ACCOUNT_H_
