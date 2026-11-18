// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_mechanism.h"

#include <string>

#include "base/types/expected.h"

namespace net {

base::expected<std::string, HttpAuthMechanism::TargetPrincipalError>
HttpAuthMechanism::GetTargetPrincipalHost() const {
  return base::unexpected(TargetPrincipalError::kNotReported);
}

bool HttpAuthMechanism::ResetSecurityContext() {
  return false;
}

}  // namespace net
