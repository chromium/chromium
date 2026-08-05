// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/idp_accounts_parser.h"

#include "base/containers/flat_set.h"
#include "base/strings/string_util.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/metrics.h"
#include "content/public/browser/webid/constants.h"
#include "url/gurl.h"

namespace content::webid {

namespace {

using AccountsResponseInvalidReason =
    IdpNetworkRequestManager::AccountsResponseInvalidReason;

bool IsEmptyOrWhitespace(const std::string* input) {
  return !input || base::TrimWhitespaceASCII(*input, base::TRIM_ALL).empty();
}

scoped_refptr<IdentityRequestAccount> ParseAccount(
    const base::DictValue& account) {
  auto* id = account.FindString(kAccountIdKey);
  auto* email = account.FindString(kAccountEmailKey);
  auto* name = account.FindString(kAccountNameKey);
  auto* phone = account.FindString(kAccountPhoneNumberKey);
  auto* username = account.FindString(kAccountUsernameKey);
  auto* given_name = account.FindString(kAccountGivenNameKey);
  auto* picture = account.FindString(kAccountPictureKey);
  auto* approved_clients = account.FindList(kAccountApprovedClientsKey);
  auto* potentially_approved_site_hashes =
      account.FindList(kPotentiallyApprovedSiteHashes);

  std::vector<std::string> account_hints;
  if (auto* hints = account.FindList(kHintsKey)) {
    for (const base::Value& entry : *hints) {
      if (entry.is_string()) {
        account_hints.emplace_back(entry.GetString());
      }
    }
  }

  std::vector<std::string> domain_hints;
  if (auto* domain_hints_list = account.FindList(kDomainHintsKey)) {
    for (const base::Value& entry : *domain_hints_list) {
      if (entry.is_string()) {
        domain_hints.emplace_back(entry.GetString());
      }
    }
  }

  std::vector<std::string> labels;
  if (auto* labels_list = account.FindList(kLabelHintsKey)) {
    for (const base::Value& entry : *labels_list) {
      if (entry.is_string()) {
        labels.emplace_back(entry.GetString());
      }
    }
  }

  if (!id) {
    return nullptr;
  }

  std::string display_identifier;
  std::string display_name;
  std::string empty_string;

  std::vector<std::string_view> identifiers;
  if (!IsEmptyOrWhitespace(name)) {
    identifiers.emplace_back(*name);
  } else {
    name = &empty_string;
  }
  if (!IsEmptyOrWhitespace(username)) {
    identifiers.emplace_back(*username);
  }
  if (!IsEmptyOrWhitespace(email)) {
    // TODO(crbug.com/40849405): validate email address.
    identifiers.emplace_back(*email);
  } else {
    email = &empty_string;
  }
  if (!IsEmptyOrWhitespace(phone)) {
    identifiers.emplace_back(*phone);
  }
  if (identifiers.empty()) {
    return nullptr;
  }
  display_name = identifiers[0];
  if (identifiers.size() > 1) {
    display_identifier = identifiers[1];
  }

  RecordApprovedClientsExistence(approved_clients != nullptr);

  std::optional<std::vector<std::string>> approved_clients_list;
  if (approved_clients) {
    approved_clients_list = std::vector<std::string>();
    for (const base::Value& entry : *approved_clients) {
      if (entry.is_string()) {
        approved_clients_list->push_back(entry.GetString());
      }
    }
    RecordApprovedClientsSize(approved_clients->size());
  }

  std::vector<std::string> potentially_approved_site_hashes_vector;
  if (IsEmbedderInitiatedLoginEnabled() && potentially_approved_site_hashes) {
    for (const base::Value& entry : *potentially_approved_site_hashes) {
      if (entry.is_string()) {
        potentially_approved_site_hashes_vector.push_back(entry.GetString());
      }
    }
  }

  auto parsed_account = base::MakeRefCounted<IdentityRequestAccount>(
      *id, display_identifier, display_name, *email, *name,
      given_name ? *given_name : "", picture ? GURL(*picture) : GURL(),
      phone ? *phone : "", username ? *username : "",
      std::move(potentially_approved_site_hashes_vector),
      std::move(account_hints), std::move(domain_hints), std::move(labels),
      /*idp_claimed_login_state=*/std::nullopt,
      /*browser_trusted_login_state=*/
      IdentityRequestAccount::LoginState::kSignUp);
  parsed_account->approved_clients = std::move(approved_clients_list);
  return parsed_account;
}

}  // namespace

// static
IdpAccountsParser::ParseResult IdpAccountsParser::ParseAccounts(
    const base::DictValue& response_dict) {
  const base::ListValue* accounts = response_dict.FindList(kAccountsKey);
  if (!accounts) {
    return base::unexpected(AccountsResponseInvalidReason::kNoAccountsKey);
  }

  if (accounts->empty()) {
    return base::unexpected(AccountsResponseInvalidReason::kAccountListIsEmpty);
  }

  std::vector<scoped_refptr<IdentityRequestAccount>> account_list;
  base::flat_set<std::string> account_ids;
  for (const auto& account : *accounts) {
    const base::DictValue* account_dict = account.GetIfDict();
    if (!account_dict) {
      return base::unexpected(AccountsResponseInvalidReason::kAccountIsNotDict);
    }

    scoped_refptr<IdentityRequestAccount> parsed_account =
        ParseAccount(*account_dict);
    if (!parsed_account) {
      return base::unexpected(
          AccountsResponseInvalidReason::kAccountMissesRequiredField);
    }

    if (account_ids.contains(parsed_account->id)) {
      return base::unexpected(
          AccountsResponseInvalidReason::kAccountsShareSameId);
    }

    account_ids.insert(parsed_account->id);
    account_list.push_back(std::move(parsed_account));
  }

  return account_list;
}

}  // namespace content::webid
