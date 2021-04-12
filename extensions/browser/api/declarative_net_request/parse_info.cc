// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/parse_info.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/notreached.h"
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

std::string GetError(ParseResult error_reason, const int* rule_id) {
  // Every error except ERROR_PERSISTING_RULESET requires |rule_id|.
  DCHECK_EQ(!rule_id, error_reason == ParseResult::ERROR_PERSISTING_RULESET);

  switch (error_reason) {
    case ParseResult::NONE:
      break;
    case ParseResult::SUCCESS:
      break;
    case ParseResult::ERROR_REQUEST_METHOD_DUPLICATED:
      return ErrorUtils::FormatErrorMessage(kErrorRequestMethodDuplicated,
                                            base::NumberToString(*rule_id));
    case ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED:
      return ErrorUtils::FormatErrorMessage(kErrorResourceTypeDuplicated,
                                            base::NumberToString(*rule_id));
    case ParseResult::ERROR_INVALID_RULE_ID:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidRuleKey, base::NumberToString(*rule_id), kIDKey,
          base::NumberToString(kMinValidID));
    case ParseResult::ERROR_INVALID_RULE_PRIORITY:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidRuleKey, base::NumberToString(*rule_id), kPriorityKey,
          base::NumberToString(kMinValidPriority));
    case ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES:
      return ErrorUtils::FormatErrorMessage(kErrorNoApplicableResourceTypes,

                                            base::NumberToString(*rule_id));
    case ParseResult::ERROR_EMPTY_DOMAINS_LIST:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(*rule_id), kDomainsKey);
    case ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(*rule_id), kResourceTypesKey);
    case ParseResult::ERROR_EMPTY_REQUEST_METHODS_LIST:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(*rule_id), kRequestMethodsKey);
    case ParseResult::ERROR_EMPTY_URL_FILTER:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyKey, base::NumberToString(*rule_id), kUrlFilterKey);
    case ParseResult::ERROR_INVALID_REDIRECT_URL:
      return ErrorUtils::FormatErrorMessage(kErrorInvalidRedirectUrl,
                                            base::NumberToString(*rule_id),
                                            kRedirectUrlPath);
    case ParseResult::ERROR_DUPLICATE_IDS:
      return ErrorUtils::FormatErrorMessage(kErrorDuplicateIDs,
                                            base::NumberToString(*rule_id));
    case ParseResult::ERROR_PERSISTING_RULESET:
      return kErrorPersisting;
    case ParseResult::ERROR_NON_ASCII_URL_FILTER:
      return ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(*rule_id), kUrlFilterKey);
    case ParseResult::ERROR_NON_ASCII_DOMAIN:
      return ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(*rule_id), kDomainsKey);
    case ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN:
      return ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(*rule_id), kExcludedDomainsKey);
    case ParseResult::ERROR_INVALID_URL_FILTER:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(*rule_id), kUrlFilterKey);
    case ParseResult::ERROR_INVALID_REDIRECT:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(*rule_id), kRedirectPath);
    case ParseResult::ERROR_INVALID_EXTENSION_PATH:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(*rule_id), kExtensionPathPath);
    case ParseResult::ERROR_INVALID_TRANSFORM_SCHEME:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidTransformScheme, base::NumberToString(*rule_id),
          kTransformSchemePath,
          JoinString(base::span<const char* const>(kAllowedTransformSchemes)));
    case ParseResult::ERROR_INVALID_TRANSFORM_PORT:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(*rule_id), kTransformPortPath);
    case ParseResult::ERROR_INVALID_TRANSFORM_QUERY:
      return ErrorUtils::FormatErrorMessage(kErrorInvalidKey,
                                            base::NumberToString(*rule_id),
                                            kTransformQueryPath);
    case ParseResult::ERROR_INVALID_TRANSFORM_FRAGMENT:
      return ErrorUtils::FormatErrorMessage(kErrorInvalidKey,
                                            base::NumberToString(*rule_id),
                                            kTransformFragmentPath);
    case ParseResult::ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED:
      return ErrorUtils::FormatErrorMessage(
          kErrorQueryAndTransformBothSpecified, base::NumberToString(*rule_id),
          kTransformQueryPath, kTransformQueryTransformPath);
    case ParseResult::ERROR_JAVASCRIPT_REDIRECT:
      return ErrorUtils::FormatErrorMessage(kErrorJavascriptRedirect,
                                            base::NumberToString(*rule_id),
                                            kRedirectUrlPath);
    case ParseResult::ERROR_EMPTY_REGEX_FILTER:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyKey, base::NumberToString(*rule_id), kRegexFilterKey);
    case ParseResult::ERROR_NON_ASCII_REGEX_FILTER:
      return ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(*rule_id), kRegexFilterKey);
    case ParseResult::ERROR_INVALID_REGEX_FILTER:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(*rule_id), kRegexFilterKey);
    case ParseResult::ERROR_NO_HEADERS_SPECIFIED:
      return ErrorUtils::FormatErrorMessage(
          kErrorNoHeaderListsSpecified, base::NumberToString(*rule_id),
          kRequestHeadersPath, kResponseHeadersPath);
    case ParseResult::ERROR_EMPTY_REQUEST_HEADERS_LIST:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(*rule_id), kRequestHeadersPath);
    case ParseResult::ERROR_EMPTY_RESPONSE_HEADERS_LIST:
      return ErrorUtils::FormatErrorMessage(kErrorEmptyList,
                                            base::NumberToString(*rule_id),
                                            kResponseHeadersPath);
    case ParseResult::ERROR_INVALID_HEADER_NAME:
      return ErrorUtils::FormatErrorMessage(kErrorInvalidHeaderName,
                                            base::NumberToString(*rule_id));
    case ParseResult::ERROR_INVALID_HEADER_VALUE:
      return ErrorUtils::FormatErrorMessage(kErrorInvalidHeaderValue,
                                            base::NumberToString(*rule_id));
    case ParseResult::ERROR_HEADER_VALUE_NOT_SPECIFIED:
      return ErrorUtils::FormatErrorMessage(kErrorNoHeaderValueSpecified,
                                            base::NumberToString(*rule_id));
    case ParseResult::ERROR_HEADER_VALUE_PRESENT:
      return ErrorUtils::FormatErrorMessage(kErrorHeaderValuePresent,
                                            base::NumberToString(*rule_id));
    case ParseResult::ERROR_APPEND_REQUEST_HEADER_UNSUPPORTED:
      return ErrorUtils::FormatErrorMessage(kErrorCannotAppendRequestHeader,
                                            base::NumberToString(*rule_id));
    case ParseResult::ERROR_REGEX_TOO_LARGE:
      // These rules are ignored while indexing and so won't cause an error.
      break;
    case ParseResult::ERROR_MULTIPLE_FILTERS_SPECIFIED:
      return ErrorUtils::FormatErrorMessage(kErrorMultipleFilters,
                                            base::NumberToString(*rule_id),
                                            kUrlFilterKey, kRegexFilterKey);
    case ParseResult::ERROR_REGEX_SUBSTITUTION_WITHOUT_FILTER:
      return ErrorUtils::FormatErrorMessage(
          kErrorRegexSubstitutionWithoutFilter, base::NumberToString(*rule_id),
          kRegexSubstitutionKey, kRegexFilterKey);
    case ParseResult::ERROR_INVALID_REGEX_SUBSTITUTION:
      return ErrorUtils::FormatErrorMessage(kErrorInvalidKey,
                                            base::NumberToString(*rule_id),
                                            kRegexSubstitutionPath);
    case ParseResult::ERROR_INVALID_ALLOW_ALL_REQUESTS_RESOURCE_TYPE:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidAllowAllRequestsResourceType,
          base::NumberToString(*rule_id));
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

ParseInfo::ParseInfo(size_t rules_count,
                     size_t regex_rules_count,
                     std::vector<int> regex_limit_exceeded_rules,
                     flatbuffers::DetachedBuffer buffer,
                     int ruleset_checksum)
    : has_error_(false),
      rules_count_(rules_count),
      regex_rules_count_(regex_rules_count),
      regex_limit_exceeded_rules_(std::move(regex_limit_exceeded_rules)),
      buffer_(std::move(buffer)),
      ruleset_checksum_(ruleset_checksum) {}

ParseInfo::ParseInfo(ParseResult error_reason, const int* rule_id)
    : has_error_(true),
      error_(GetError(error_reason, rule_id)),
      error_reason_(error_reason) {}

ParseInfo::ParseInfo(ParseInfo&&) = default;
ParseInfo& ParseInfo::operator=(ParseInfo&&) = default;
ParseInfo::~ParseInfo() = default;

}  // namespace declarative_net_request
}  // namespace extensions
