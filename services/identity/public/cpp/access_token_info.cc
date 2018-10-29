// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/access_token_info.h"

namespace identity {

bool operator==(const AccessTokenInfo& lhs, const AccessTokenInfo& rhs) {
  return (lhs.token == rhs.token) &&
         (lhs.expiration_time == rhs.expiration_time) &&
         (lhs.id_token == rhs.id_token);
}

}  // namespace identity
