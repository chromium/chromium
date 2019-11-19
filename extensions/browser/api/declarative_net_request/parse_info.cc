// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/parse_info.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "extensions/common/error_utils.h"

namespace extensions {
namespace declarative_net_request {

namespace {

// Helper to ensure pointers to string literals can be used with
// base::JoinString.
std::string JoinString(base::span<const char* const> parts) {
  std::vector<base::StringPiece> parts_piece;
  for (const char* part : parts)
    parts_piece.push_back(part);
  return base::JoinString(parts_piece, ", ");
}

}  // namespace

ParseInfo::ParseInfo(ParseResult result) : result_(result) {}
ParseInfo::ParseInfo(ParseResult result, int rule_id)
    : result_(result), rule_id_(rule_id) {}
ParseInfo::ParseInfo(const ParseInfo&) = default;
ParseInfo& ParseInfo::operator=(const ParseInfo&) = default;

std::string ParseInfo::GetErrorDescription() const {
  // Every error except ERROR_PERSISTING_RULESET requires |rule_id_|.
  DCHECK_EQ(!rule_id_.has_value(),
            result_ == ParseResult::ERROR_PERSISTING_RULESET);

  std::string error;
  switch (result_) {
    case ParseResult::SUCCESS:
      NOTREACHED();
      break;
    case ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED:
      error = ErrorUtils::FormatErrorMessage(kErrorResourceTypeDuplicated,
                                             base::NumberToString(*rule_id_));
      break;
    case ParseResult::ERROR_EMPTY_REDIRECT_RULE_PRIORITY:
      error = ErrorUtils::FormatErrorMessage(kErrorEmptyRedirectRuleKey,
                                             base::NumberToString(*rule_id_),
                                             kPriorityKey);
      break;
    case ParseResult::ERROR_EMPTY_UPGRADE_RULE_PRIORITY:
      error = ErrorUtils::FormatErrorMessage(kErrorEmptyUpgradeRulePriority,
                                             base::NumberToString(*rule_id_));
      break;
    case ParseResult::ERROR_INVALID_RULE_ID:
      error = ErrorUtils::FormatErrorMessage(
          kErrorInvalidRuleKey, base::NumberToString(*rule_id_), kIDKey,
          base::NumberToString(kMinValidID));
      break;
    case ParseResult::ERROR_INVALID_REDIRECT_RULE_PRIORITY:
    case ParseResult::ERROR_INVALID_UPGRADE_RULE_PRIORITY:
      error = ErrorUtils::FormatErrorMessage(
          kErrorInvalidRuleKey, base::NumberToString(*rule_id_), kPriorityKey,
          base::NumberToString(kMinValidPriority));
      break;
    case ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES:
      error = ErrorUtils::FormatErrorMessage(kErrorNoApplicableResourceTypes,

                                             base::NumberToString(*rule_id_));
      break;
    case ParseResult::ERROR_EMPTY_DOMAINS_LIST:
      error = ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(*rule_id_), kDomainsKey);
      break;
    case ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST:
      error = ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(*rule_id_), kResourceTypesKey);
      break;
    case ParseResult::ERROR_EMPTY_URL_FILTER:
      error = ErrorUtils::FormatErrorMessage(
          kErrorEmptyKey, base::NumberToString(*rule_id_), kUrlFilterKey);
      break;
    case ParseResult::ERROR_INVALID_REDIRECT_URL:
      error = ErrorUtils::FormatErrorMessage(kErrorInvalidRedirectUrl,
                                             base::NumberToString(*rule_id_),
                                             kRedirectUrlPath);
      break;
    case ParseResult::ERROR_DUPLICATE_IDS:
      error = ErrorUtils::FormatErrorMessage(kErrorDuplicateIDs,
                                             base::NumberToString(*rule_id_));
      break;
    case ParseResult::ERROR_PERSISTING_RULESET:
      error = kErrorPersisting;
      break;
    case ParseResult::ERROR_NON_ASCII_URL_FILTER:
      error = ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(*rule_id_), kUrlFilterKey);
      break;
    case ParseResult::ERROR_NON_ASCII_DOMAIN:
      error = ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(*rule_id_), kDomainsKey);
      break;
    case ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN:
      error = ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(*rule_id_), kExcludedDomainsKey);
      break;
    case ParseResult::ERROR_INVALID_URL_FILTER:
      error = ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(*rule_id_), kUrlFilterKey);
      break;
    case ParseResult::ERROR_EMPTY_REMOVE_HEADERS_LIST:
      error = ErrorUtils::FormatErrorMessage(kErrorEmptyRemoveHeadersList,
                                             base::NumberToString(*rule_id_),
                                             kRemoveHeadersListKey);
      break;
    case ParseResult::ERROR_INVALID_REDIRECT:
      error = ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(*rule_id_), kRedirectPath);
      break;
    case ParseResult::ERROR_INVALID_EXTENSION_PATH:
      error = ErrorUtils::FormatErrorMessage(kErrorInvalidKey,
                                             base::NumberToString(*rule_id_),
                                             kExtensionPathPath);
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_SCHEME:
      error = ErrorUtils::FormatErrorMessage(
          kErrorInvalidTransformScheme, base::NumberToString(*rule_id_),
          kTransformSchemePath,
          JoinString(base::span<const char* const>(kAllowedTransformSchemes)));
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_PORT:
      error = ErrorUtils::FormatErrorMessage(kErrorInvalidKey,
                                             base::NumberToString(*rule_id_),
                                             kTransformPortPath);
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_QUERY:
      error = ErrorUtils::FormatErrorMessage(kErrorInvalidKey,
                                             base::NumberToString(*rule_id_),
                                             kTransformQueryPath);
      break;
    case ParseResult::ERROR_INVALID_TRANSFORM_FRAGMENT:
      error = ErrorUtils::FormatErrorMessage(kErrorInvalidKey,
                                             base::NumberToString(*rule_id_),
                                             kTransformFragmentPath);
      break;
    case ParseResult::ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED:
      error = ErrorUtils::FormatErrorMessage(
          kErrorQueryAndTransformBothSpecified, base::NumberToString(*rule_id_),
          kTransformQueryPath, kTransformQueryTransformPath);
      break;
    case ParseResult::ERROR_JAVASCRIPT_REDIRECT:
      error = ErrorUtils::FormatErrorMessage(kErrorJavascriptRedirect,
                                             base::NumberToString(*rule_id_),
                                             kRedirectUrlPath);
      break;
    case ParseResult::ERROR_EMPTY_REGEX_FILTER:
      error = ErrorUtils::FormatErrorMessage(
          kErrorEmptyKey, base::NumberToString(*rule_id_), kRegexFilterKey);
      break;
    case ParseResult::ERROR_NON_ASCII_REGEX_FILTER:
      error = ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(*rule_id_), kRegexFilterKey);
      break;
    case ParseResult::ERROR_INVALID_REGEX_FILTER:
      error = ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(*rule_id_), kRegexFilterKey);
      break;
    case ParseResult::ERROR_MULTIPLE_FILTERS_SPECIFIED:
      error = ErrorUtils::FormatErrorMessage(kErrorMultipleFilters,
                                             base::NumberToString(*rule_id_),
                                             kUrlFilterKey, kRegexFilterKey);
      break;
  }
  return error;
}

}  // namespace declarative_net_request
}  // namespace extensions
