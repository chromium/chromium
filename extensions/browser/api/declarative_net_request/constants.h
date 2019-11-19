// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_CONSTANTS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

#include "extensions/common/api/declarative_net_request/constants.h"

namespace extensions {
namespace declarative_net_request {

// The result of parsing JSON rules provided by an extension. Can correspond to
// a single or multiple rules.
enum class ParseResult {
  SUCCESS,
  ERROR_RESOURCE_TYPE_DUPLICATED,
  ERROR_EMPTY_REDIRECT_RULE_PRIORITY,
  ERROR_EMPTY_UPGRADE_RULE_PRIORITY,
  ERROR_INVALID_RULE_ID,
  ERROR_INVALID_REDIRECT_RULE_PRIORITY,
  ERROR_INVALID_UPGRADE_RULE_PRIORITY,
  ERROR_NO_APPLICABLE_RESOURCE_TYPES,
  ERROR_EMPTY_DOMAINS_LIST,
  ERROR_EMPTY_RESOURCE_TYPES_LIST,
  ERROR_EMPTY_URL_FILTER,
  ERROR_INVALID_REDIRECT_URL,
  ERROR_DUPLICATE_IDS,
  ERROR_PERSISTING_RULESET,

  // Parse errors related to fields containing non-ascii characters.
  ERROR_NON_ASCII_URL_FILTER,
  ERROR_NON_ASCII_DOMAIN,
  ERROR_NON_ASCII_EXCLUDED_DOMAIN,

  ERROR_INVALID_URL_FILTER,
  ERROR_EMPTY_REMOVE_HEADERS_LIST,
  ERROR_INVALID_REDIRECT,
  ERROR_INVALID_EXTENSION_PATH,
  ERROR_INVALID_TRANSFORM_SCHEME,
  ERROR_INVALID_TRANSFORM_PORT,
  ERROR_INVALID_TRANSFORM_QUERY,
  ERROR_INVALID_TRANSFORM_FRAGMENT,
  ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED,
  ERROR_JAVASCRIPT_REDIRECT,
  ERROR_EMPTY_REGEX_FILTER,
  ERROR_NON_ASCII_REGEX_FILTER,
  ERROR_INVALID_REGEX_FILTER,
  ERROR_MULTIPLE_FILTERS_SPECIFIED,
};

// Describes the ways in which updating dynamic rules can fail.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UpdateDynamicRulesStatus {
  kSuccess = 0,
  kErrorReadJSONRules = 1,
  kErrorRuleCountExceeded = 2,
  kErrorCreateTemporarySource = 3,
  kErrorWriteTemporaryJSONRuleset = 4,
  kErrorWriteTemporaryIndexedRuleset = 5,
  kErrorInvalidRules = 6,
  kErrorCreateDynamicRulesDirectory = 7,
  kErrorReplaceIndexedFile = 8,
  kErrorReplaceJSONFile = 9,
  kErrorCreateMatcher_InvalidPath = 10,
  kErrorCreateMatcher_FileReadError = 11,
  kErrorCreateMatcher_ChecksumMismatch = 12,
  kErrorCreateMatcher_VersionMismatch = 13,

  // Magic constant used by histograms code. Should be equal to the largest enum
  // value.
  kMaxValue = kErrorCreateMatcher_VersionMismatch,
};

// Schemes which can be used as part of url transforms.
extern const char* const kAllowedTransformSchemes[4];

// Rule parsing errors.
extern const char kErrorResourceTypeDuplicated[];
extern const char kErrorEmptyRedirectRuleKey[];
extern const char kErrorEmptyUpgradeRulePriority[];
extern const char kErrorInvalidRuleKey[];
extern const char kErrorNoApplicableResourceTypes[];
extern const char kErrorEmptyList[];
extern const char kErrorEmptyKey[];
extern const char kErrorInvalidRedirectUrl[];
extern const char kErrorDuplicateIDs[];
extern const char kErrorPersisting[];
extern const char kErrorNonAscii[];
extern const char kErrorEmptyRemoveHeadersList[];
extern const char kErrorInvalidKey[];
extern const char kErrorInvalidTransformScheme[];
extern const char kErrorQueryAndTransformBothSpecified[];
extern const char kErrorJavascriptRedirect[];
extern const char kErrorMultipleFilters[];

extern const char kErrorListNotPassed[];

// Rule indexing install warnings.
extern const char kRuleCountExceeded[];
extern const char kRuleNotParsedWarning[];
extern const char kTooManyParseFailuresWarning[];

// Dynamic rules API errors.
extern const char kInternalErrorUpdatingDynamicRules[];
extern const char kInternalErrorGettingDynamicRules[];
extern const char kDynamicRuleCountExceeded[];

// Histogram names.
extern const char kIndexAndPersistRulesTimeHistogram[];
extern const char kManifestRulesCountHistogram[];
extern const char kUpdateDynamicRulesStatusHistogram[];
extern const char kReadDynamicRulesJSONStatusHistogram[];

// Placeholder text to use for getBadgeText extension function call, when the
// badge text is set to the DNR action count.
extern const char kActionCountPlaceholderBadgeText[];

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_CONSTANTS_H_
