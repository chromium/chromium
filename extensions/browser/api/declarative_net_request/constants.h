// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_CONSTANTS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_CONSTANTS_H_

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "extensions/common/api/declarative_net_request/constants.h"

namespace extensions::declarative_net_request {

// The result of parsing JSON rules provided by an extension. Corresponds to a
// single rule.
enum class ParseResult {
  NONE,
  SUCCESS,
  ERROR_REQUEST_METHOD_DUPLICATED,
  ERROR_RESOURCE_TYPE_DUPLICATED,
  ERROR_INVALID_RULE_ID,
  ERROR_INVALID_RULE_PRIORITY,
  ERROR_NO_APPLICABLE_RESOURCE_TYPES,
  ERROR_EMPTY_DOMAINS_LIST,
  ERROR_EMPTY_INITIATOR_DOMAINS_LIST,
  ERROR_EMPTY_REQUEST_DOMAINS_LIST,
  ERROR_DOMAINS_AND_INITIATOR_DOMAINS_BOTH_SPECIFIED,
  ERROR_EXCLUDED_DOMAINS_AND_EXCLUDED_INITIATOR_DOMAINS_BOTH_SPECIFIED,
  ERROR_EMPTY_RESOURCE_TYPES_LIST,
  ERROR_EMPTY_REQUEST_METHODS_LIST,
  ERROR_EMPTY_URL_FILTER,
  ERROR_INVALID_REDIRECT_URL,
  ERROR_DUPLICATE_IDS,

  // Parse errors related to fields containing non-ascii characters.
  ERROR_NON_ASCII_URL_FILTER,
  ERROR_NON_ASCII_DOMAIN,
  ERROR_NON_ASCII_EXCLUDED_DOMAIN,
  ERROR_NON_ASCII_INITIATOR_DOMAIN,
  ERROR_NON_ASCII_EXCLUDED_INITIATOR_DOMAIN,
  ERROR_NON_ASCII_REQUEST_DOMAIN,
  ERROR_NON_ASCII_EXCLUDED_REQUEST_DOMAIN,

  ERROR_INVALID_URL_FILTER,
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
  ERROR_REGEX_TOO_LARGE,
  ERROR_MULTIPLE_FILTERS_SPECIFIED,
  ERROR_REGEX_SUBSTITUTION_WITHOUT_FILTER,
  ERROR_INVALID_REGEX_SUBSTITUTION,
  ERROR_INVALID_ALLOW_ALL_REQUESTS_RESOURCE_TYPE,

  // Parse errors related to fields specific to modifyheaders rules.
  ERROR_NO_HEADERS_TO_MODIFY_SPECIFIED,
  ERROR_EMPTY_MODIFY_REQUEST_HEADERS_LIST,
  ERROR_EMPTY_MODIFY_RESPONSE_HEADERS_LIST,
  ERROR_INVALID_HEADER_TO_MODIFY_NAME,
  ERROR_INVALID_HEADER_TO_MODIFY_VALUE,
  ERROR_HEADER_VALUE_NOT_SPECIFIED,
  ERROR_HEADER_VALUE_PRESENT,
  ERROR_APPEND_INVALID_REQUEST_HEADER,

  // Parse errors related to matching on tab IDs.
  ERROR_EMPTY_TAB_IDS_LIST,
  ERROR_TAB_IDS_ON_NON_SESSION_RULE,
  ERROR_TAB_ID_DUPLICATED,

  // Parse errors related to matching on response headers.
  ERROR_EMPTY_RESPONSE_HEADER_MATCHING_LIST,
  ERROR_EMPTY_EXCLUDED_RESPONSE_HEADER_MATCHING_LIST,
  ERROR_INVALID_MATCHING_RESPONSE_HEADER_NAME,
  ERROR_INVALID_MATCHING_EXCLUDED_RESPONSE_HEADER_NAME,
  ERROR_INVALID_MATCHING_RESPONSE_HEADER_VALUE,
  ERROR_MATCHING_RESPONSE_HEADER_DUPLICATED,
  ERROR_RESPONSE_HEADER_RULE_CANNOT_MODIFY_REQUEST_HEADERS,
};

// Describes the ways in which updating dynamic rules can fail.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UpdateDynamicRulesStatus {
  kSuccess = 0,
  kErrorReadJSONRules = 1,
  kErrorRuleCountExceeded = 2,
  // kErrorCreateTemporarySource_Deprecated = 3,
  // kErrorWriteTemporaryJSONRuleset_Deprecated = 4,
  // kErrorWriteTemporaryIndexedRuleset_Deprecated = 5,
  kErrorInvalidRules = 6,
  kErrorCreateDynamicRulesDirectory = 7,
  // kErrorReplaceIndexedFile_Deprecated = 8,
  // kErrorReplaceJSONFile_Deprecated = 9,
  kErrorCreateMatcher_InvalidPath = 10,
  kErrorCreateMatcher_FileReadError = 11,
  kErrorCreateMatcher_ChecksumMismatch = 12,
  kErrorCreateMatcher_VersionMismatch = 13,
  kErrorRegexTooLarge = 14,
  kErrorRegexRuleCountExceeded = 15,
  kErrorSerializeToJson = 16,
  kErrorWriteJson = 17,
  kErrorWriteFlatbuffer = 18,
  kErrorUnsafeRuleCountExceeded = 19,

  // Magic constant used by histograms code. Should be equal to the largest enum
  // value.
  kMaxValue = kErrorUnsafeRuleCountExceeded,
};

// Describes the result of loading a single JSON Ruleset.
// This is logged as part of UMA. Hence existing values should not be re-
// numbered or deleted.
enum class LoadRulesetResult {
  // Ruleset loading succeeded.
  kSuccess = 0,

  // Ruleset loading failed since the provided path did not exist.
  kErrorInvalidPath = 1,

  // Ruleset loading failed due to a file read error.
  kErrorCannotReadFile = 2,

  // Ruleset loading failed due to a checksum mismatch.
  kErrorChecksumMismatch = 3,

  // Ruleset loading failed due to version header mismatch.
  // TODO(karandeepb): This should be split into two cases:
  //    - When the indexed ruleset doesn't have the version header in the
  //      correct format.
  //    - When the indexed ruleset's version is not the same as that used by
  //      Chrome.
  kErrorVersionMismatch = 4,

  // Ruleset loading failed since the checksum for the ruleset wasn't found in
  // prefs.
  kErrorChecksumNotFound = 5,

  // Magic constant used by histograms code. Should be equal to the largest enum
  // value.
  kMaxValue = kErrorChecksumNotFound,
};

// Specifies whether and how extensions require host permissions to modify the
// request.
enum class HostPermissionsAlwaysRequired {
  // In this case, all actions require host permissions to the request url and
  // initiator.
  kTrue,
  // In this case, only redirecting (excluding upgrading) requests and modifying
  // headers require host permissions to the request url and initiator.
  kFalse,
};

// Specifies the request stage for which rulesets are to be matched.
enum class RulesetMatchingStage {
  // At this stage, the request has been prepared but not sent. Rules with
  // conditions based only on the request's parameters will be matched.
  kOnBeforeRequest,
  // At this stage, the request's headers have been sent and response headers
  // have been received. Rules with conditions that depend on the request's
  // response will be matched.
  kOnHeadersReceived,
};

// Schemes which can be used as part of url transforms.
extern const char* const kAllowedTransformSchemes[4];

// Rule parsing errors.
extern const char kErrorRequestMethodDuplicated[];
extern const char kErrorResourceTypeDuplicated[];
extern const char kErrorInvalidRuleKey[];
extern const char kErrorNoApplicableResourceTypes[];
extern const char kErrorEmptyList[];
extern const char kErrorEmptyKey[];
extern const char kErrorInvalidRedirectUrl[];
extern const char kErrorDuplicateIDs[];
extern const char kErrorPersisting[];
extern const char kErrorNonAscii[];
extern const char kErrorInvalidKey[];
extern const char kErrorInvalidTransformScheme[];
extern const char kErrorQueryAndTransformBothSpecified[];
extern const char kErrorDomainsAndInitiatorDomainsBothSpecified[];
extern const char kErrorJavascriptRedirect[];
extern const char kErrorMultipleFilters[];
extern const char kErrorRegexSubstitutionWithoutFilter[];
extern const char kErrorInvalidAllowAllRequestsResourceType[];
extern const char kErrorRegexTooLarge[];
extern const char kErrorNoHeaderListsSpecified[];
extern const char kErrorInvalidModifyHeaderName[];
extern const char kErrorInvalidModifyHeaderValue[];
extern const char kErrorNoHeaderValueSpecified[];
extern const char kErrorHeaderValuePresent[];
extern const char kErrorAppendInvalidRequestHeader[];
extern const char kErrorTabIdsOnNonSessionRule[];
extern const char kErrorTabIdDuplicated[];
extern const char kErrorInvalidMatchingHeaderName[];
extern const char kErrorInvalidMatchingHeaderValue[];
extern const char kErrorResponseHeaderDuplicated[];
extern const char kErrorResponseHeaderRuleCannotModifyRequestHeaders[];

extern const char kErrorListNotPassed[];

// Rule indexing install warnings.
extern const char kRuleCountExceeded[];
extern const char kRegexRuleCountExceeded[];
extern const char kEnabledRuleCountExceeded[];
extern const char kEnabledRegexRuleCountExceeded[];
extern const char kRuleNotParsedWarning[];
extern const char kTooManyParseFailuresWarning[];
extern const char kIndexingRuleLimitExceeded[];

// Dynamic rules API errors.
extern const char kInternalErrorUpdatingDynamicRules[];
extern const char kInternalErrorGettingDynamicRules[];
extern const char kDynamicRuleCountExceeded[];
extern const char kDynamicUnsafeRuleCountExceeded[];
extern const char kDynamicRegexRuleCountExceeded[];

// Session-scoped rules API errors.
extern const char kSessionRuleCountExceeded[];
extern const char kSessionUnsafeRuleCountExceeded[];
extern const char kSessionRegexRuleCountExceeded[];

// Static ruleset toggling API errors.
extern const char kInvalidRulesetIDError[];
extern const char kEnabledRulesetsRuleCountExceeded[];
extern const char kEnabledRulesetsRegexRuleCountExceeded[];
extern const char kInternalErrorUpdatingEnabledRulesets[];
extern const char kEnabledRulesetCountExceeded[];

// Static rule toggling API errors.
extern const char kDisabledStaticRuleCountExceeded[];

// setExtensionActionOptions API errors.
extern const char kTabNotFoundError[];
extern const char kIncrementActionCountWithoutUseAsBadgeTextError[];

// testMatchOutcome API errors.
extern const char kInvalidTestURLError[];
extern const char kInvalidTestInitiatorError[];
extern const char kInvalidTestTabIdError[];
extern const char kInvalidResponseHeaderObjectError[];
extern const char kInvalidResponseHeaderNameError[];
extern const char kInvalidResponseHeaderValueError[];

// Histogram names.
extern const char kIndexAndPersistRulesTimeHistogram[];
extern const char kManifestEnabledRulesCountHistogram[];
extern const char kUpdateDynamicRulesStatusHistogram[];
extern const char kReadDynamicRulesJSONStatusHistogram[];
extern const char kIsLargeRegexHistogram[];
extern const char kRegexRuleSizeHistogram[];
extern const char kLoadRulesetResultHistogram[];

// Placeholder text to use for getBadgeText extension function call, when the
// badge text is set to the DNR action count.
extern const char kActionCountPlaceholderBadgeText[];

// Error returned for the getMatchedRules extension function call, if the
// extension does not have sufficient permissions to make the call.
extern const char kErrorGetMatchedRulesMissingPermissions[];

// The maximum amount of static rules in the global rule pool for a single
// profile.
inline constexpr int kMaxStaticRulesPerProfile = 300000;

// The per-extension maximum amount of disabled static rules.
inline constexpr int kMaxDisabledStaticRules = 5000;

// Maximum size of a compiled RegEx rule in KB. Limited to 2 KB which means
// that given 1024 rules, the total usage would be 2 MB.
inline constexpr int kRegexMaxMemKb = 2;

// Identifier for a Flatbuffer containing `flat::EmbedderConditions` as the
// root.
extern const char kEmbedderConditionsBufferIdentifier[];

// An allowlist of request headers that can be appended onto, in the form of
// (header name, header delimiter). Currently, this list contains all standard
// HTTP request headers that support multiple values in a single entry. This
// list may be extended in the future to support custom headers.
inline constexpr auto kDNRRequestHeaderAppendAllowList =
    base::MakeFixedFlatMap<std::string_view, std::string_view>(
        {{"accept", ", "},
         {"accept-encoding", ", "},
         {"accept-language", ", "},
         {"access-control-request-headers", ", "},
         {"cache-control", ", "},
         {"connection", ", "},
         {"content-language", ", "},
         {"cookie", "; "},
         {"forwarded", ", "},
         {"if-match", ", "},
         {"if-none-match", ", "},
         {"keep-alive", ", "},
         {"range", ", "},
         {"te", ", "},
         {"trailer", ""},
         {"transfer-encoding", ", "},
         {"upgrade", ", "},
         {"user-agent", " "},
         {"via", ", "},
         {"want-digest", ", "},
         {"x-forwarded-for", ", "}});

}  // namespace extensions::declarative_net_request

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_CONSTANTS_H_
