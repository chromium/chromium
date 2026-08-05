// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_IDP_ACCOUNTS_PARSER_H_
#define CONTENT_BROWSER_WEBID_IDP_ACCOUNTS_PARSER_H_

#include <optional>
#include <vector>

#include "base/types/expected.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/webid/identity_request_account.h"

namespace content::webid {

// Parser for the FedCM accounts endpoint response dictionary.
class CONTENT_EXPORT IdpAccountsParser {
 public:
  IdpAccountsParser() = delete;

  static constexpr char kAccountsKey[] = "accounts";

  using ParseResult =
      base::expected<std::vector<scoped_refptr<IdentityRequestAccount>>,
                     IdpNetworkRequestManager::AccountsResponseInvalidReason>;

  // Parses accounts from a JSON dictionary response.
  static ParseResult ParseAccounts(const base::DictValue& response_dict);
};

}  // namespace content::webid

#endif  // CONTENT_BROWSER_WEBID_IDP_ACCOUNTS_PARSER_H_
