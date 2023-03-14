// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_OAUTH2_ID_TOKEN_DECODER_H_
#define GOOGLE_APIS_GAIA_OAUTH2_ID_TOKEN_DECODER_H_

#include <string>
#include <vector>

#include "base/component_export.h"

// This file holds methods decodes the id token received for OAuth2 token
// endpoint, and derive useful information from it, such as whether the account
// is a child account, and whether this account is under advanced protection.

namespace gaia {

// Service flags extracted from ID token.
struct COMPONENT_EXPORT(GOOGLE_APIS) TokenServiceFlags {
  bool is_child_account = false;
  bool is_under_advanced_protection = false;
};

// Parses service flag from ID token.
COMPONENT_EXPORT(GOOGLE_APIS)
TokenServiceFlags ParseServiceFlags(const std::string& id_token);

}  // namespace gaia

#endif  // GOOGLE_APIS_GAIA_OAUTH2_ID_TOKEN_DECODER_H_
