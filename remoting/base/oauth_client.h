// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_OAUTH_CLIENT_H_
#define REMOTING_BASE_OAUTH_CLIENT_H_

#include <string>

#include "base/callback.h"

namespace gaia {
struct OAuthClientInfo;
}

namespace remoting {

class OAuthClient {
 public:
  // Called when GetCredentialsFromAuthCode is completed, with the |user_email|
  // and |refresh_token| that correspond to the given |auth_code|, or with empty
  // strings on error.
  typedef base::OnceCallback<void(const std::string& user_email,
                                  const std::string& refresh_token)>
      CompletionCallback;

  virtual ~OAuthClient() {}

  // Redeems |auth_code| using |oauth_client_info| to obtain
  // |refresh_token| and |access_token|, then, if |need_user_email| is
  // true, uses the userinfo endpoint to obtain |user_email|.  Calls
  // CompletionCallback with |user_email| and |refresh_token| when
  // done, or with empty strings on error.  If a request is received
  // while another one is being processed, it is enqueued and
  // processed after the first one is finished.
  virtual void GetCredentialsFromAuthCode(
      const gaia::OAuthClientInfo& oauth_client_info,
      const std::string& auth_code,
      bool need_user_email,
      CompletionCallback on_done) = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_OAUTH_CLIENT_H_
