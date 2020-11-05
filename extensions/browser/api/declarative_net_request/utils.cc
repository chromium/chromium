// Copyright 2017 The Chromium Authors. All rights reserved.
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
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/dnr_manifest_data.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace extensions {
namespace declarative_net_request {
namespace {

namespace dnr_api = api::declarative_net_request;

// The ruleset format version of the flatbuffer schema. Increment this whenever
// making an incompatible change to the schema at extension_ruleset.fbs or
// url_pattern_index.fbs. Whenever an extension with an indexed ruleset format
// version different from the one currently used by Chrome is loaded, the
// extension ruleset will be reindexed.
constexpr int kIndexedRulesetFormatVersion = 18;

// This static assert is meant to catch cases where
// url_pattern_index::kUrlPatternIndexFormatVersion is incremented without
// updating kIndexedRulesetFormatVersion.
static_assert(url_pattern_index::kUrlPatternIndexFormatVersion == 6,
              "kUrlPatternIndexFormatVersion has changed, make sure you've "
              "also updated kIndexedRulesetFormatVersion above.");

constexpr int kInvalidIndexedRulesetFormatVersion = -1;
int g_indexed_ruleset_format_version_for_testing =
    kInvalidIndexedRulesetFormatVersion;

constexpr int kInvalidOverrideChecksumForTest = -1;
int g_override_checksum_for_test = kInvalidOverrideChecksumForTest;

constexpr int kInvalidRuleLimit = -1;
int g_static_guaranteed_minimum_for_testing = kInvalidRuleLimit;
int g_static_rule_limit_for_testing = kInvalidRuleLimit;
int g_global_static_rule_limit_for_testing = kInvalidRuleLimit;
int g_regex_rule_limit_for_testing = kInvalidRuleLimit;

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

}  // namespace

bool IsValidRulesetData(base::span<const uint8_t> data, int expected_checksum) {
  flatbuffers::Verifier verifier(data.data(), data.size());
  return expected_checksum == GetChecksum(data) &&
         flat::VerifyExtensionIndexedRulesetBuffer(verifier);
}

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

bool PersistIndexedRuleset(const base::FilePath& path,
                           base::span<const uint8_t> data,
                           int* ruleset_checksum) {
  DCHECK(ruleset_checksum);

  // Create the directory corresponding to |path| if it does not exist.
  if (!base::CreateDirectory(path.DirName()))
    return false;

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

  *ruleset_checksum = GetChecksum(data);
  return true;
}

void ClearRendererCacheOnNavigation() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  web_cache::WebCacheManager::GetInstance()->ClearCacheOnNavigation();
}

void LogReadDynamicRulesStatus(ReadJSONRulesResult::Status status) {
  base::UmaHistogramEnumeration(kReadDynamicRulesJSONStatusHistogram, status);
}

// Maps WebRequestResourceType to api::declarative_net_request::ResourceType.
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
  }
  NOTREACHED();
  return dnr_api::RESOURCE_TYPE_OTHER;
}

dnr_api::RequestDetails CreateRequestDetails(const WebRequestInfo& request) {
  api::declarative_net_request::RequestDetails details;
  details.request_id = base::NumberToString(request.id);
  details.url = request.url.spec();

  if (request.initiator) {
    details.initiator =
        std::make_unique<std::string>(request.initiator->Serialize());
  }

  details.method = request.method;
  details.frame_id = request.frame_data.frame_id;
  details.parent_frame_id = request.frame_data.parent_frame_id;
  details.tab_id = request.frame_data.tab_id;
  details.type = GetDNRResourceType(request.web_request_type);
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

  DCHECK_GE(ruleset_id, kMinValidStaticRulesetID);
  return DNRManifestData::GetRuleset(extension, ruleset_id).manifest_id;
}

std::vector<std::string> GetPublicRulesetIDs(const Extension& extension,
                                             const CompositeMatcher& matcher) {
  std::vector<std::string> ids;
  ids.reserve(matcher.matchers().size());
  for (const std::unique_ptr<RulesetMatcher>& matcher : matcher.matchers())
    ids.push_back(GetPublicRulesetID(extension, matcher->id()));

  return ids;
}

int GetStaticGuaranteedMinimumRuleCount() {
  return g_static_guaranteed_minimum_for_testing == kInvalidRuleLimit
             ? dnr_api::GUARANTEED_MINIMUM_STATIC_RULES
             : g_static_guaranteed_minimum_for_testing;
}

int GetGlobalStaticRuleLimit() {
  DCHECK(base::FeatureList::IsEnabled(kDeclarativeNetRequestGlobalRules));
  return g_global_static_rule_limit_for_testing == kInvalidRuleLimit
             ? kMaxStaticRulesPerProfile
             : g_global_static_rule_limit_for_testing;
}

int GetStaticRuleLimit() {
  DCHECK(!base::FeatureList::IsEnabled(kDeclarativeNetRequestGlobalRules));
  return g_static_rule_limit_for_testing == kInvalidRuleLimit
             ? dnr_api::MAX_NUMBER_OF_RULES
             : g_static_rule_limit_for_testing;
}

int GetMaximumRulesPerRuleset() {
  return base::FeatureList::IsEnabled(kDeclarativeNetRequestGlobalRules)
             ? GetStaticGuaranteedMinimumRuleCount() +
                   GetGlobalStaticRuleLimit()
             : GetStaticRuleLimit();
}

int GetDynamicRuleLimit() {
  return dnr_api::MAX_NUMBER_OF_DYNAMIC_RULES;
}

int GetRegexRuleLimit() {
  return g_regex_rule_limit_for_testing == kInvalidRuleLimit
             ? dnr_api::MAX_NUMBER_OF_REGEX_RULES
             : g_regex_rule_limit_for_testing;
}

ScopedRuleLimitOverride CreateScopedStaticGuaranteedMinimumOverrideForTesting(
    int minimum) {
  return base::AutoReset<int>(&g_static_guaranteed_minimum_for_testing,
                              minimum);
}

ScopedRuleLimitOverride CreateScopedStaticRuleLimitOverrideForTesting(
    int limit) {
  return base::AutoReset<int>(&g_static_rule_limit_for_testing, limit);
}

ScopedRuleLimitOverride CreateScopedGlobalStaticRuleLimitOverrideForTesting(
    int limit) {
  return base::AutoReset<int>(&g_global_static_rule_limit_for_testing, limit);
}

ScopedRuleLimitOverride CreateScopedRegexRuleLimitOverrideForTesting(
    int limit) {
  return base::AutoReset<int>(&g_regex_rule_limit_for_testing, limit);
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

}  // namespace declarative_net_request
}  // namespace extensions
