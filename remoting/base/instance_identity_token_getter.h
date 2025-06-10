// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_INSTANCE_IDENTITY_TOKEN_GETTER_H_
#define REMOTING_BASE_INSTANCE_IDENTITY_TOKEN_GETTER_H_

#include <string_view>

#include "base/functional/callback_forward.h"

namespace remoting {

// Interface for retrieving instance identity tokens for Compute Engine service
// requests.
class InstanceIdentityTokenGetter {
 public:
  using TokenCallback = base::OnceCallback<void(std::string_view)>;

  InstanceIdentityTokenGetter() = default;

  InstanceIdentityTokenGetter(const InstanceIdentityTokenGetter&) = delete;
  InstanceIdentityTokenGetter& operator=(const InstanceIdentityTokenGetter&) =
      delete;

  virtual ~InstanceIdentityTokenGetter() = default;

  // Calls |on_token| with an identity token, or empty in the case the request
  // fails. The token returned has a lifetime of at least 10 minutes and should
  // not be cached.
  virtual void RetrieveToken(TokenCallback on_token) = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_INSTANCE_IDENTITY_TOKEN_GETTER_H_
