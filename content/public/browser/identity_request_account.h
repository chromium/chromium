// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_ACCOUNT_H_
#define CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_ACCOUNT_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace content {

// Represents a federated user account which is used when displaying the FedCM
// account selector.
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
    // This represents the auto re-authn flow. Currently it's only available
    // when RP specifies |autoReauthn = true| AND there is only one signed in
    // account.
    kAuto,
  };

  IdentityRequestAccount(
      const std::string& id,
      const std::string& email,
      const std::string& name,
      const std::string& given_name,
      const GURL& picture,
      std::vector<std::string> hints,
      absl::optional<LoginState> login_state = absl::nullopt);
  IdentityRequestAccount(const IdentityRequestAccount&);
  ~IdentityRequestAccount();

  std::string id;
  std::string email;
  std::string name;
  std::string given_name;
  GURL picture;
  std::vector<std::string> hints;

  // The account login state. Unlike the other fields this one can be populated
  // either by the IDP or by the browser based on its stored permission grants.
  absl::optional<LoginState> login_state;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_IDENTITY_REQUEST_ACCOUNT_H_
