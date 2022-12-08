// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/utils.h"

#include <memory>
#include <set>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/url_pattern_index/url_pattern_index.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/api/web_request/web_request_resource_type.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace extensions {
namespace declarative_net_request {
namespace {

namespace dnr_api = api::declarative_net_request;
namespace flat_rule = url_pattern_index::flat;

// The ruleset format version of the flatbuffer schema. Increment this whenever
// making an incompatible change to the schema at extension_ruleset.fbs or
// url_pattern_index.fbs. Whenever an extension with an indexed ruleset format
// version different from the one currently used by Chrome is loaded, the
// extension ruleset will be reindexed.
constexpr int kIndexedRulesetFormatVersion = 27;

// This static assert is meant to catch cases where
// url_pattern_index::kUrlPatternIndexFormatVersion is incremented without
// updating kIndexedRulesetFormatVersion.
static_assert(url_pattern_index::kUrlPatternIndexFormatVersion == 14,
              "kUrlPatternIndexFormatVersion has changed, make sure you've "
              "also updated kIndexedRulesetFormatVersion above.");

constexpr int kInvalidIndexedRulesetFormatVersion = -1;
int g_indexed_ruleset_format_version_for_testing =
    kInvalidIndexedRulesetFormatVersion;

constexpr int kInvalidOverrideChecksumForTest = -1;
int g_override_checksum_for_test = kInvalidOverrideChecksumForTest;

constexpr int kInvalidRuleLimit = -1;
int g_static_guaranteed_minimum_for_testing = kInvalidRuleLimit;
int g_global_static_rule_limit_for_testing = kInvalidRuleLimit;
int g_regex_rule_limit_for_testing = kInvalidRuleLimit;
int g_dynamic_and_session_rule_limit_for_testing = kInvalidRuleLimit;
int g_disabled_static_rule_limit_for_testing = kInvalidRuleLimit;

int GetIndexedRulesetFormatVersion() {
  return g_indexed_ruleset_format_version_for_testing ==
                 kInvalidIndexedRulesetFormatVersion
             ? kIndexedRulesetFormatVersion
             : g_indexed_ruleset_format_version_for_testing;
}

// Returns the header to be used for indexed rulesets. This depends on the
// current ruleset format version.
std::string GetVersionHeader() {
  return base::StringPrintf("---------Version=%d",
                            GetIndexedRulesetFormatVersion());
}

// Helper to ensure pointers to string literals can be used with
// base::JoinString.
std::string JoinString(base::span<const char* const> parts) {
  std::vector<base::StringPiece> parts_piece;
  for (const char* part : parts)
    parts_piece.push_back(part);
  return base::JoinString(parts_piece, ", ");
}

}  // namespace

std::string GetVersionHeaderForTesting() {
  return GetVersionHeader();
}

int GetIndexedRulesetFormatVersionForTesting() {
  return GetIndexedRulesetFormatVersion();
}

ScopedIncrementRulesetVersion CreateScopedIncrementRulesetVersionForTesting() {
  return base::AutoReset<int>(&g_indexed_ruleset_format_version_for_testing,
                              GetIndexedRulesetFormatVersion() + 1);
}

bool StripVersionHeaderAndParseVersion(std::string* ruleset_data) {
  DCHECK(ruleset_data);
  const std::string version_header = GetVersionHeader();

  if (!base::StartsWith(*ruleset_data, version_header,
                        base::CompareCase::SENSITIVE)) {
    return false;
  }

  // Strip the header from |ruleset_data|.
  ruleset_data->erase(0, version_header.size());
  return true;
}

int GetChecksum(base::span<const uint8_t> data) {
  if (g_override_checksum_for_test != kInvalidOverrideChecksumForTest)
    return g_override_checksum_for_test;

  uint32_t hash = base::PersistentHash(data.data(), data.size());

  // Strip off the sign bit since this needs to be persisted in preferences
  // which don't support unsigned ints.
  return static_cast<int>(hash & 0x7fffffff);
}

void OverrideGetChecksumForTest(int checksum) {
  g_override_checksum_for_test = checksum;
}

std::string GetIndexedRulesetData(base::span<const uint8_t> data) {
  return base::StrCat(
      {GetVersionHeader(),
       base::StringPiece(reinterpret_cast<const char*>(data.data()),
                         data.size())});
}

bool PersistIndexedRuleset(const base::FilePath& path,
                           base::span<const uint8_t> data) {
  // Create the directory corresponding to |path| if it does not exist.
  if (!base::CreateDirectory(path.DirName()))
    return false;

  // Unlike for dynamic rules, we don't use `ImportantFileWriter` here since it
  // can be quite slow (and this will be called for the extension's indexed
  // static rulesets). Also the file persisting logic here is simpler than for
  // dynamic rules where we need to persist both the JSON and indexed rulesets
  // and keep them in sync.
  base::File ruleset_file(
      path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!ruleset_file.IsValid())
    return false;

  // Write the version header.
  std::string version_header = GetVersionHeader();
  int version_header_size = static_cast<int>(version_header.size());
  if (ruleset_file.WriteAtCurrentPos(
          version_header.data(), version_header_size) != version_header_size) {
    return false;
  }

  // Write the flatbuffer ruleset.
  if (!base::IsValueInRangeForNumericType<int>(data.size()))
    return false;
  int data_size = static_cast<int>(data.size());
  if (ruleset_file.WriteAtCurrentPos(reinterpret_cast<const char*>(data.data()),
                                     data_size) != data_size) {
    return false;
  }

  return true;
}

void ClearRendererCacheOnNavigation() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  extensions::ExtensionsBrowserClient::Get()->ClearBackForwardCache();
  web_cache::WebCacheManager::GetInstance()->ClearCacheOnNavigation();
}

void LogReadDynamicRulesStatus(ReadJSONRulesResult::Status status) {
  base::UmaHistogramEnumeration(kReadDynamicRulesJSONStatusHistogram, status);
}

// Maps WebRequestResourceType to dnr_api::ResourceType.
dnr_api::ResourceType GetDNRResourceType(WebRequestResourceType resource_type) {
  switch (resource_type) {
    case WebRequestResourceType::OTHER:
      return dnr_api::RESOURCE_TYPE_OTHER;
    case WebRequestResourceType::MAIN_FRAME:
      return dnr_api::RESOURCE_TYPE_MAIN_FRAME;
    case WebRequestResourceType::CSP_REPORT:
      return dnr_api::RESOURCE_TYPE_CSP_REPORT;
    case WebRequestResourceType::SCRIPT:
      return dnr_api::RESOURCE_TYPE_SCRIPT;
    case WebRequestResourceType::IMAGE:
      return dnr_api::RESOURCE_TYPE_IMAGE;
    case WebRequestResourceType::STYLESHEET:
      return dnr_api::RESOURCE_TYPE_STYLESHEET;
    case WebRequestResourceType::OBJECT:
      return dnr_api::RESOURCE_TYPE_OBJECT;
    case WebRequestResourceType::XHR:
      return dnr_api::RESOURCE_TYPE_XMLHTTPREQUEST;
    case WebRequestResourceType::SUB_FRAME:
      return dnr_api::RESOURCE_TYPE_SUB_FRAME;
    case WebRequestResourceType::PING:
      return dnr_api::RESOURCE_TYPE_PING;
    case WebRequestResourceType::MEDIA:
      return dnr_api::RESOURCE_TYPE_MEDIA;
    case WebRequestResourceType::FONT:
      return dnr_api::RESOURCE_TYPE_FONT;
    case WebRequestResourceType::WEB_SOCKET:
      return dnr_api::RESOURCE_TYPE_WEBSOCKET;
    case WebRequestResourceType::WEB_TRANSPORT:
      return dnr_api::RESOURCE_TYPE_WEBTRANSPORT;
    case WebRequestResourceType::WEBBUNDLE:
      return dnr_api::RESOURCE_TYPE_WEBBUNDLE;
  }
  NOTREACHED();
  return dnr_api::RESOURCE_TYPE_OTHER;
}

// Maps dnr_api::ResourceType to WebRequestResourceType.
WebRequestResourceType GetWebRequestResourceType(
    dnr_api::ResourceType resource_type) {
  switch (resource_type) {
    case dnr_api::RESOURCE_TYPE_OTHER:
      return WebRequestResourceType::OTHER;
    case dnr_api::RESOURCE_TYPE_MAIN_FRAME:
      return WebRequestResourceType::MAIN_FRAME;
    case dnr_api::RESOURCE_TYPE_CSP_REPORT:
      return WebRequestResourceType::CSP_REPORT;
    case dnr_api::RESOURCE_TYPE_SCRIPT:
      return WebRequestResourceType::SCRIPT;
    case dnr_api::RESOURCE_TYPE_IMAGE:
      return WebRequestResourceType::IMAGE;
    case dnr_api::RESOURCE_TYPE_STYLESHEET:
      return WebRequestResourceType::STYLESHEET;
    case dnr_api::RESOURCE_TYPE_OBJECT:
      return WebRequestResourceType::OBJECT;
    case dnr_api::RESOURCE_TYPE_XMLHTTPREQUEST:
      return WebRequestResourceType::XHR;
    case dnr_api::RESOURCE_TYPE_SUB_FRAME:
      return WebRequestResourceType::SUB_FRAME;
    case dnr_api::RESOURCE_TYPE_PING:
      return WebRequestResourceType::PING;
    case dnr_api::RESOURCE_TYPE_MEDIA:
      return WebRequestResourceType::MEDIA;
    case dnr_api::RESOURCE_TYPE_FONT:
      return WebRequestResourceType::FONT;
    case dnr_api::RESOURCE_TYPE_WEBSOCKET:
      return WebRequestResourceType::WEB_SOCKET;
    case dnr_api::RESOURCE_TYPE_WEBTRANSPORT:
      return WebRequestResourceType::WEB_TRANSPORT;
    case dnr_api::RESOURCE_TYPE_WEBBUNDLE:
      return WebRequestResourceType::WEBBUNDLE;
    case dnr_api::RESOURCE_TYPE_NONE:
      NOTREACHED();
      return WebRequestResourceType::OTHER;
  }
  NOTREACHED();
  return WebRequestResourceType::OTHER;
}

dnr_api::RequestDetails CreateRequestDetails(const WebRequestInfo& request) {
  dnr_api::RequestDetails details;
  details.request_id = base::NumberToString(request.id);
  details.url = request.url.spec();

  if (request.initiator) {
    details.initiator = request.initiator->Serialize();
  }

  details.method = request.method;
  details.frame_id = request.frame_data.frame_id;
  if (request.frame_data.document_id) {
    details.document_id = request.frame_data.document_id.ToString();
  }
  details.parent_frame_id = request.frame_data.parent_frame_id;
  if (request.frame_data.parent_document_id) {
    details.parent_document_id =
        request.frame_data.parent_document_id.ToString();
  }
  details.tab_id = request.frame_data.tab_id;
  details.type = GetDNRResourceType(request.web_request_type);
  details.frame_type = request.frame_data.frame_type;
  details.document_lifecycle = request.frame_data.document_lifecycle;
  return details;
}

re2::RE2::Options CreateRE2Options(bool is_case_sensitive,
                                   bool require_capturing) {
  re2::RE2::Options options;

  // RE2 supports UTF-8 and Latin1 encoding. We only need to support ASCII, so
  // use Latin1 encoding. This should also be more efficient than UTF-8.
  // Note: Latin1 is an 8 bit extension to ASCII.
  options.set_encoding(re2::RE2::Options::EncodingLatin1);

  options.set_case_sensitive(is_case_sensitive);

  // Don't capture unless needed, for efficiency.
  options.set_never_capture(!require_capturing);

  options.set_log_errors(false);

  // Limit the maximum memory per regex to 2 Kb. This means given 1024 rules,
  // the total usage would be 2 Mb.
  options.set_max_mem(2 << 10);

  return options;
}

flat::ActionType ConvertToFlatActionType(dnr_api::RuleActionType action_type) {
  switch (action_type) {
    case dnr_api::RULE_ACTION_TYPE_BLOCK:
      return flat::ActionType_block;
    case dnr_api::RULE_ACTION_TYPE_ALLOW:
      return flat::ActionType_allow;
    case dnr_api::RULE_ACTION_TYPE_REDIRECT:
      return flat::ActionType_redirect;
    case dnr_api::RULE_ACTION_TYPE_MODIFYHEADERS:
      return flat::ActionType_modify_headers;
    case dnr_api::RULE_ACTION_TYPE_UPGRADESCHEME:
      return flat::ActionType_upgrade_scheme;
    case dnr_api::RULE_ACTION_TYPE_ALLOWALLREQUESTS:
      return flat::ActionType_allow_all_requests;
    case dnr_api::RULE_ACTION_TYPE_NONE:
      break;
  }
  NOTREACHED();
  return flat::ActionType_block;
}

std::string GetPublicRulesetID(const Extension& extension,
                               RulesetID ruleset_id) {
  if (ruleset_id == kDynamicRulesetID)
    return dnr_api::DYNAMIC_RULESET_ID;
  if (ruleset_id == kSessionRulesetID)
    return dnr_api::SESSION_RULESET_ID;

  DCHECK_GE(ruleset_id, kMinValidStaticRulesetID);
  return DNRManifestData::GetRuleset(extension, ruleset_id).manifest_id;
}

std::vector<std::string> GetPublicRulesetIDs(const Extension& extension,
                                             const CompositeMatcher& matcher) {
  std::vector<std::string> ids;
  ids.reserve(matcher.matchers().size());
  for (const std::unique_ptr<RulesetMatcher>& ruleset_matcher :
       matcher.matchers())
    ids.push_back(GetPublicRulesetID(extension, ruleset_matcher->id()));

  return ids;
}

int GetStaticGuaranteedMinimumRuleCount() {
  return g_static_guaranteed_minimum_for_testing == kInvalidRuleLimit
             ? dnr_api::GUARANTEED_MINIMUM_STATIC_RULES
             : g_static_guaranteed_minimum_for_testing;
}

int GetGlobalStaticRuleLimit() {
  return g_global_static_rule_limit_for_testing == kInvalidRuleLimit
             ? kMaxStaticRulesPerProfile
             : g_global_static_rule_limit_for_testing;
}

int GetMaximumRulesPerRuleset() {
  return GetStaticGuaranteedMinimumRuleCount() + GetGlobalStaticRuleLimit();
}

int GetDynamicAndSessionRuleLimit() {
  return g_dynamic_and_session_rule_limit_for_testing == kInvalidRuleLimit
             ? dnr_api::MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES
             : g_dynamic_and_session_rule_limit_for_testing;
}

int GetRegexRuleLimit() {
  return g_regex_rule_limit_for_testing == kInvalidRuleLimit
             ? dnr_api::MAX_NUMBER_OF_REGEX_RULES
             : g_regex_rule_limit_for_testing;
}

int GetDisabledStaticRuleLimit() {
  return g_disabled_static_rule_limit_for_testing == kInvalidRuleLimit
             ? kMaxDisabledStaticRules
             : g_disabled_static_rule_limit_for_testing;
}

ScopedRuleLimitOverride CreateScopedStaticGuaranteedMinimumOverrideForTesting(
    int minimum) {
  return base::AutoReset<int>(&g_static_guaranteed_minimum_for_testing,
                              minimum);
}

ScopedRuleLimitOverride CreateScopedGlobalStaticRuleLimitOverrideForTesting(
    int limit) {
  return base::AutoReset<int>(&g_global_static_rule_limit_for_testing, limit);
}

ScopedRuleLimitOverride CreateScopedRegexRuleLimitOverrideForTesting(
    int limit) {
  return base::AutoReset<int>(&g_regex_rule_limit_for_testing, limit);
}

ScopedRuleLimitOverride
CreateScopedDynamicAndSessionRuleLimitOverrideForTesting(int limit) {
  return base::AutoReset<int>(&g_dynamic_and_session_rule_limit_for_testing,
                              limit);
}

ScopedRuleLimitOverride CreateScopedDisabledStaticRuleLimitOverrideForTesting(
    int limit) {
  return base::AutoReset<int>(&g_disabled_static_rule_limit_for_testing, limit);
}

size_t GetEnabledStaticRuleCount(const CompositeMatcher* composite_matcher) {
  if (!composite_matcher)
    return 0;

  size_t enabled_static_rule_count = 0;
  for (const std::unique_ptr<RulesetMatcher>& matcher :
       composite_matcher->matchers()) {
    if (matcher->id() == kDynamicRulesetID)
      continue;

    enabled_static_rule_count += matcher->GetRulesCount();
  }

  return enabled_static_rule_count;
}

bool HasDNRFeedbackPermission(const Extension* extension,
                              const absl::optional<int>& tab_id) {
  const PermissionsData* permissions_data = extension->permissions_data();
  return tab_id.has_value()
             ? permissions_data->HasAPIPermissionForTab(
                   *tab_id,
                   mojom::APIPermissionID::kDeclarativeNetRequestFeedback)
             : permissions_data->HasAPIPermission(
                   mojom::APIPermissionID::kDeclarativeNetRequestFeedback);
}

// TODO(crbug.com/1370166): Add a parameter that allows more specific strings
// for error messages that can pinpoint the error within a single rule.
std::string GetParseError(ParseResult error_reason, int rule_id) {
  switch (error_reason) {
    case ParseResult::NONE:
      break;
    case ParseResult::SUCCESS:
      break;
    case ParseResult::ERROR_REQUEST_METHOD_DUPLICATED:
      return ErrorUtils::FormatErrorMessage(kErrorRequestMethodDuplicated,
                                            base::NumberToString(rule_id));
    case ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED:
      return ErrorUtils::FormatErrorMessage(kErrorResourceTypeDuplicated,
                                            base::NumberToString(rule_id));
    case ParseResult::ERROR_INVALID_RULE_ID:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidRuleKey, base::NumberToString(rule_id), kIDKey,
          base::NumberToString(kMinValidID));
    case ParseResult::ERROR_INVALID_RULE_PRIORITY:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidRuleKey, base::NumberToString(rule_id), kPriorityKey,
          base::NumberToString(kMinValidPriority));
    case ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES:
      return ErrorUtils::FormatErrorMessage(kErrorNoApplicableResourceTypes,

                                            base::NumberToString(rule_id));
    case ParseResult::ERROR_EMPTY_DOMAINS_LIST:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(rule_id), kDomainsKey);
    case ParseResult::ERROR_EMPTY_INITIATOR_DOMAINS_LIST:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(rule_id), kInitiatorDomainsKey);
    case ParseResult::ERROR_EMPTY_REQUEST_DOMAINS_LIST:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(rule_id), kRequestDomainsKey);
    case ParseResult::ERROR_DOMAINS_AND_INITIATOR_DOMAINS_BOTH_SPECIFIED:
      return ErrorUtils::FormatErrorMessage(
          kErrorDomainsAndInitiatorDomainsBothSpecified,
          base::NumberToString(rule_id), kDomainsKey, kInitiatorDomainsKey);
    case ParseResult::
        ERROR_EXCLUDED_DOMAINS_AND_EXCLUDED_INITIATOR_DOMAINS_BOTH_SPECIFIED:
      return ErrorUtils::FormatErrorMessage(
          kErrorDomainsAndInitiatorDomainsBothSpecified,
          base::NumberToString(rule_id), kExcludedDomainsKey,
          kExcludedInitiatorDomainsKey);
    case ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(rule_id), kResourceTypesKey);
    case ParseResult::ERROR_EMPTY_REQUEST_METHODS_LIST:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(rule_id), kRequestMethodsKey);
    case ParseResult::ERROR_EMPTY_URL_FILTER:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyKey, base::NumberToString(rule_id), kUrlFilterKey);
    case ParseResult::ERROR_INVALID_REDIRECT_URL:
      return ErrorUtils::FormatErrorMessage(kErrorInvalidRedirectUrl,
                                            base::NumberToString(rule_id),
                                            kRedirectUrlPath);
    case ParseResult::ERROR_DUPLICATE_IDS:
      return ErrorUtils::FormatErrorMessage(kErrorDuplicateIDs,
                                            base::NumberToString(rule_id));
    case ParseResult::ERROR_NON_ASCII_URL_FILTER:
      return ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(rule_id), kUrlFilterKey);
    case ParseResult::ERROR_NON_ASCII_DOMAIN:
      return ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(rule_id), kDomainsKey);
    case ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN:
      return ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(rule_id), kExcludedDomainsKey);
    case ParseResult::ERROR_NON_ASCII_INITIATOR_DOMAIN:
      return ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(rule_id), kInitiatorDomainsKey);
    case ParseResult::ERROR_NON_ASCII_EXCLUDED_INITIATOR_DOMAIN:
      return ErrorUtils::FormatErrorMessage(kErrorNonAscii,
                                            base::NumberToString(rule_id),
                                            kExcludedInitiatorDomainsKey);
    case ParseResult::ERROR_NON_ASCII_REQUEST_DOMAIN:
      return ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(rule_id), kRequestDomainsKey);
    case ParseResult::ERROR_NON_ASCII_EXCLUDED_REQUEST_DOMAIN:
      return ErrorUtils::FormatErrorMessage(kErrorNonAscii,
                                            base::NumberToString(rule_id),
                                            kExcludedRequestDomainsKey);
    case ParseResult::ERROR_INVALID_URL_FILTER:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(rule_id), kUrlFilterKey);
    case ParseResult::ERROR_INVALID_REDIRECT:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(rule_id), kRedirectPath);
    case ParseResult::ERROR_INVALID_EXTENSION_PATH:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(rule_id), kExtensionPathPath);
    case ParseResult::ERROR_INVALID_TRANSFORM_SCHEME:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidTransformScheme, base::NumberToString(rule_id),
          kTransformSchemePath,
          JoinString(base::span<const char* const>(kAllowedTransformSchemes)));
    case ParseResult::ERROR_INVALID_TRANSFORM_PORT:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(rule_id), kTransformPortPath);
    case ParseResult::ERROR_INVALID_TRANSFORM_QUERY:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(rule_id), kTransformQueryPath);
    case ParseResult::ERROR_INVALID_TRANSFORM_FRAGMENT:
      return ErrorUtils::FormatErrorMessage(kErrorInvalidKey,
                                            base::NumberToString(rule_id),
                                            kTransformFragmentPath);
    case ParseResult::ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED:
      return ErrorUtils::FormatErrorMessage(
          kErrorQueryAndTransformBothSpecified, base::NumberToString(rule_id),
          kTransformQueryPath, kTransformQueryTransformPath);
    case ParseResult::ERROR_JAVASCRIPT_REDIRECT:
      return ErrorUtils::FormatErrorMessage(kErrorJavascriptRedirect,
                                            base::NumberToString(rule_id),
                                            kRedirectUrlPath);
    case ParseResult::ERROR_EMPTY_REGEX_FILTER:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyKey, base::NumberToString(rule_id), kRegexFilterKey);
    case ParseResult::ERROR_NON_ASCII_REGEX_FILTER:
      return ErrorUtils::FormatErrorMessage(
          kErrorNonAscii, base::NumberToString(rule_id), kRegexFilterKey);
    case ParseResult::ERROR_INVALID_REGEX_FILTER:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidKey, base::NumberToString(rule_id), kRegexFilterKey);
    case ParseResult::ERROR_NO_HEADERS_SPECIFIED:
      return ErrorUtils::FormatErrorMessage(
          kErrorNoHeaderListsSpecified, base::NumberToString(rule_id),
          kRequestHeadersPath, kResponseHeadersPath);
    case ParseResult::ERROR_EMPTY_REQUEST_HEADERS_LIST:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(rule_id), kRequestHeadersPath);
    case ParseResult::ERROR_EMPTY_RESPONSE_HEADERS_LIST:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(rule_id), kResponseHeadersPath);
    case ParseResult::ERROR_INVALID_HEADER_NAME:
      return ErrorUtils::FormatErrorMessage(kErrorInvalidHeaderName,
                                            base::NumberToString(rule_id));
    case ParseResult::ERROR_INVALID_HEADER_VALUE:
      return ErrorUtils::FormatErrorMessage(kErrorInvalidHeaderValue,
                                            base::NumberToString(rule_id));
    case ParseResult::ERROR_HEADER_VALUE_NOT_SPECIFIED:
      return ErrorUtils::FormatErrorMessage(kErrorNoHeaderValueSpecified,
                                            base::NumberToString(rule_id));
    case ParseResult::ERROR_HEADER_VALUE_PRESENT:
      return ErrorUtils::FormatErrorMessage(kErrorHeaderValuePresent,
                                            base::NumberToString(rule_id));
    case ParseResult::ERROR_APPEND_INVALID_REQUEST_HEADER:
      return ErrorUtils::FormatErrorMessage(kErrorAppendInvalidRequestHeader,
                                            base::NumberToString(rule_id));
    case ParseResult::ERROR_REGEX_TOO_LARGE:
      return ErrorUtils::FormatErrorMessage(
          kErrorRegexTooLarge, base::NumberToString(rule_id), kRegexFilterKey);
    case ParseResult::ERROR_MULTIPLE_FILTERS_SPECIFIED:
      return ErrorUtils::FormatErrorMessage(kErrorMultipleFilters,
                                            base::NumberToString(rule_id),
                                            kUrlFilterKey, kRegexFilterKey);
    case ParseResult::ERROR_REGEX_SUBSTITUTION_WITHOUT_FILTER:
      return ErrorUtils::FormatErrorMessage(
          kErrorRegexSubstitutionWithoutFilter, base::NumberToString(rule_id),
          kRegexSubstitutionKey, kRegexFilterKey);
    case ParseResult::ERROR_INVALID_REGEX_SUBSTITUTION:
      return ErrorUtils::FormatErrorMessage(kErrorInvalidKey,
                                            base::NumberToString(rule_id),
                                            kRegexSubstitutionPath);
    case ParseResult::ERROR_INVALID_ALLOW_ALL_REQUESTS_RESOURCE_TYPE:
      return ErrorUtils::FormatErrorMessage(
          kErrorInvalidAllowAllRequestsResourceType,
          base::NumberToString(rule_id));
    case ParseResult::ERROR_EMPTY_TAB_IDS_LIST:
      return ErrorUtils::FormatErrorMessage(
          kErrorEmptyList, base::NumberToString(rule_id), kTabIdsKey);
    case ParseResult::ERROR_TAB_IDS_ON_NON_SESSION_RULE:
      return ErrorUtils::FormatErrorMessage(kErrorTabIdsOnNonSessionRule,
                                            base::NumberToString(rule_id),
                                            kTabIdsKey, kExcludedTabIdsKey);
    case ParseResult::ERROR_TAB_ID_DUPLICATED:
      return ErrorUtils::FormatErrorMessage(kErrorTabIdDuplicated,
                                            base::NumberToString(rule_id));
  }
  NOTREACHED();
  return std::string();
}

flat_rule::ElementType GetElementType(WebRequestResourceType web_request_type) {
  switch (web_request_type) {
    case WebRequestResourceType::OTHER:
      return flat_rule::ElementType_OTHER;
    case WebRequestResourceType::MAIN_FRAME:
      return flat_rule::ElementType_MAIN_FRAME;
    case WebRequestResourceType::CSP_REPORT:
      return flat_rule::ElementType_CSP_REPORT;
    case WebRequestResourceType::SCRIPT:
      return flat_rule::ElementType_SCRIPT;
    case WebRequestResourceType::IMAGE:
      return flat_rule::ElementType_IMAGE;
    case WebRequestResourceType::STYLESHEET:
      return flat_rule::ElementType_STYLESHEET;
    case WebRequestResourceType::OBJECT:
      return flat_rule::ElementType_OBJECT;
    case WebRequestResourceType::XHR:
      return flat_rule::ElementType_XMLHTTPREQUEST;
    case WebRequestResourceType::SUB_FRAME:
      return flat_rule::ElementType_SUBDOCUMENT;
    case WebRequestResourceType::PING:
      return flat_rule::ElementType_PING;
    case WebRequestResourceType::MEDIA:
      return flat_rule::ElementType_MEDIA;
    case WebRequestResourceType::FONT:
      return flat_rule::ElementType_FONT;
    case WebRequestResourceType::WEBBUNDLE:
      return flat_rule::ElementType_WEBBUNDLE;
    case WebRequestResourceType::WEB_SOCKET:
      return flat_rule::ElementType_WEBSOCKET;
    case WebRequestResourceType::WEB_TRANSPORT:
      return flat_rule::ElementType_WEBTRANSPORT;
  }
  NOTREACHED();
  return flat_rule::ElementType_OTHER;
}

flat_rule::ElementType GetElementType(dnr_api::ResourceType resource_type) {
  switch (resource_type) {
    case dnr_api::RESOURCE_TYPE_NONE:
      return flat_rule::ElementType_NONE;
    case dnr_api::RESOURCE_TYPE_MAIN_FRAME:
      return flat_rule::ElementType_MAIN_FRAME;
    case dnr_api::RESOURCE_TYPE_SUB_FRAME:
      return flat_rule::ElementType_SUBDOCUMENT;
    case dnr_api::RESOURCE_TYPE_STYLESHEET:
      return flat_rule::ElementType_STYLESHEET;
    case dnr_api::RESOURCE_TYPE_SCRIPT:
      return flat_rule::ElementType_SCRIPT;
    case dnr_api::RESOURCE_TYPE_IMAGE:
      return flat_rule::ElementType_IMAGE;
    case dnr_api::RESOURCE_TYPE_FONT:
      return flat_rule::ElementType_FONT;
    case dnr_api::RESOURCE_TYPE_OBJECT:
      return flat_rule::ElementType_OBJECT;
    case dnr_api::RESOURCE_TYPE_XMLHTTPREQUEST:
      return flat_rule::ElementType_XMLHTTPREQUEST;
    case dnr_api::RESOURCE_TYPE_PING:
      return flat_rule::ElementType_PING;
    case dnr_api::RESOURCE_TYPE_CSP_REPORT:
      return flat_rule::ElementType_CSP_REPORT;
    case dnr_api::RESOURCE_TYPE_MEDIA:
      return flat_rule::ElementType_MEDIA;
    case dnr_api::RESOURCE_TYPE_WEBSOCKET:
      return flat_rule::ElementType_WEBSOCKET;
    case dnr_api::RESOURCE_TYPE_WEBTRANSPORT:
      return flat_rule::ElementType_WEBTRANSPORT;
    case dnr_api::RESOURCE_TYPE_WEBBUNDLE:
      return flat_rule::ElementType_WEBBUNDLE;
    case dnr_api::RESOURCE_TYPE_OTHER:
      return flat_rule::ElementType_OTHER;
  }
  NOTREACHED();
  return flat_rule::ElementType_NONE;
}

// Maps an HTTP request method string to flat_rule::RequestMethod.
// Returns `flat::RequestMethod_NON_HTTP` for non-HTTP(s) requests.
flat_rule::RequestMethod GetRequestMethod(bool http_or_https,
                                          const std::string& method) {
  if (!http_or_https)
    return flat_rule::RequestMethod_NON_HTTP;

  using net::HttpRequestHeaders;
  static const base::NoDestructor<
      base::flat_map<base::StringPiece, flat_rule::RequestMethod>>
      kRequestMethods(
          {{HttpRequestHeaders::kDeleteMethod, flat_rule::RequestMethod_DELETE},
           {HttpRequestHeaders::kGetMethod, flat_rule::RequestMethod_GET},
           {HttpRequestHeaders::kHeadMethod, flat_rule::RequestMethod_HEAD},
           {HttpRequestHeaders::kOptionsMethod,
            flat_rule::RequestMethod_OPTIONS},
           {HttpRequestHeaders::kPatchMethod, flat_rule::RequestMethod_PATCH},
           {HttpRequestHeaders::kPostMethod, flat_rule::RequestMethod_POST},
           {HttpRequestHeaders::kPutMethod, flat_rule::RequestMethod_PUT},
           {HttpRequestHeaders::kConnectMethod,
            flat_rule::RequestMethod_CONNECT}});

  DCHECK(base::ranges::all_of(*kRequestMethods, [](const auto& key_value) {
    return base::ranges::none_of(key_value.first, base::IsAsciiLower<char>);
  }));

  std::string normalized_method = base::ToUpperASCII(method);
  auto it = kRequestMethods->find(normalized_method);
  if (it == kRequestMethods->end()) {
    NOTREACHED() << "Request method " << normalized_method << " not handled.";
    return flat_rule::RequestMethod_GET;
  }
  return it->second;
}

flat_rule::RequestMethod GetRequestMethod(
    dnr_api::RequestMethod request_method) {
  switch (request_method) {
    case dnr_api::REQUEST_METHOD_NONE:
      NOTREACHED();
      return flat_rule::RequestMethod_NONE;
    case dnr_api::REQUEST_METHOD_CONNECT:
      return flat_rule::RequestMethod_CONNECT;
    case dnr_api::REQUEST_METHOD_DELETE:
      return flat_rule::RequestMethod_DELETE;
    case dnr_api::REQUEST_METHOD_GET:
      return flat_rule::RequestMethod_GET;
    case dnr_api::REQUEST_METHOD_HEAD:
      return flat_rule::RequestMethod_HEAD;
    case dnr_api::REQUEST_METHOD_OPTIONS:
      return flat_rule::RequestMethod_OPTIONS;
    case dnr_api::REQUEST_METHOD_PATCH:
      return flat_rule::RequestMethod_PATCH;
    case dnr_api::REQUEST_METHOD_POST:
      return flat_rule::RequestMethod_POST;
    case dnr_api::REQUEST_METHOD_PUT:
      return flat_rule::RequestMethod_PUT;
  }
  NOTREACHED();
  return flat_rule::RequestMethod_NONE;
}

flat_rule::RequestMethod GetRequestMethod(
    bool http_or_https,
    dnr_api::RequestMethod request_method) {
  if (!http_or_https)
    return flat_rule::RequestMethod_NON_HTTP;

  return GetRequestMethod(request_method);
}

}  // namespace declarative_net_request
}  // namespace extensions
