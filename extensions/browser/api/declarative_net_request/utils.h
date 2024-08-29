// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_UTILS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/web_request/web_request_resource_type.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension.h"
#include "third_party/re2/src/re2/re2.h"

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
struct WebRequestInfo;

namespace declarative_net_request {
class CompositeMatcher;

// Returns the version header used for indexed ruleset files. Only exposed for
// testing.
std::string GetVersionHeaderForTesting();

// Gets the ruleset format version for testing.
int GetIndexedRulesetFormatVersionForTesting();

// Test helper to increment the indexed ruleset format version while the
// returned value is in scope. Resets it to the original value when it goes out
// of scope.
using ScopedIncrementRulesetVersion = base::AutoReset<int>;
ScopedIncrementRulesetVersion CreateScopedIncrementRulesetVersionForTesting();

// Strips the version header from |ruleset_data|. Returns false on version
// mismatch.
bool StripVersionHeaderAndParseVersion(std::string* ruleset_data);

// Returns the checksum of the given serialized |data|. |data| must not include
// the version header.
int GetChecksum(base::span<const uint8_t> data);

// Override the result of any calls to GetChecksum() above, so that it returns
// |checksum|. Note: If |checksum| is -1, no such override is performed.
void OverrideGetChecksumForTest(int checksum);

// Returns the indexed ruleset data to be persisted to disk. The ruleset is
// composed of a version header corresponding to the current ruleset format
// version, followed by the actual ruleset data.
std::string GetIndexedRulesetData(base::span<const uint8_t> data);

// Helper function to persist the indexed ruleset |data| at the given |path|.
// The ruleset is composed of a version header corresponding to the current
// ruleset format version, followed by the actual ruleset data.
bool PersistIndexedRuleset(const base::FilePath& path,
                           base::span<const uint8_t> data);

// Helper to clear any back-forward caches and each renderer's in-memory cache
// the next time it navigates.
void ClearRendererCacheOnNavigation();

// Helper to log the |kReadDynamicRulesJSONStatusHistogram| histogram.
void LogReadDynamicRulesStatus(ReadJSONRulesResult::Status status);

// Maps dnr_api::ResourceType to WebRequestResourceType.
WebRequestResourceType GetWebRequestResourceType(
    api::declarative_net_request::ResourceType resource_type);

// Constructs an api::declarative_net_request::RequestDetails from a
// WebRequestInfo.
api::declarative_net_request::RequestDetails CreateRequestDetails(
    const WebRequestInfo& request);

// Creates default RE2::Options.
re2::RE2::Options CreateRE2Options(bool is_case_sensitive,
                                   bool require_capturing);

// Convert dnr_api::RuleActionType into flat::ActionType.
flat::ActionType ConvertToFlatActionType(
    api::declarative_net_request::RuleActionType action_type);

// Returns the extension-specified ID for the given |ruleset_id| if it
// corresponds to a static ruleset ID. For the dynamic or session-scoped ruleset
// ID, it returns the |DYNAMIC_RULESET_ID| and |SESSION_RULESET_ID| API
// constants respectively.
std::string GetPublicRulesetID(const Extension& extension,
                               RulesetID ruleset_id);

// Returns the public ruleset IDs corresponding to the given |extension| and
// |matcher|.
std::vector<std::string> GetPublicRulesetIDs(const Extension& extension,
                                             const CompositeMatcher& matcher);

// Returns the number of rules that an extension can specify across its enabled
// static rulesets that will not count towards the global total.
int GetStaticGuaranteedMinimumRuleCount();

// Returns the maximum amount of static rules in the global rule pool for a
// single profile.
int GetGlobalStaticRuleLimit();

// Returns the maximum number of rules a valid static ruleset can have. This is
// also the maximum number of static rules an extension can enable at any point.
int GetMaximumRulesPerRuleset();

// Returns the rule limit for dynamic rules. If the
// `kDeclarativeNetRequestSafeRuleLimits` is disabled, the dynamic rule limit
// will be the "unsafe" dynamic rule limit which is lower in value.
int GetDynamicRuleLimit();

// Returns the rule limit for "unsafe" dynamic rules. See the implementation for
// `IsRuleSafe` for how a rule's safety is determined.
int GetUnsafeDynamicRuleLimit();

// Returns the rule limit for session-scoped rules. If the
// `kDeclarativeNetRequestSafeRuleLimits` is disabled, the session-scoped rule
// limit will be the "unsafe" session-scoped rule limit which is lower in value.
int GetSessionRuleLimit();

// Returns the rule limit for "unsafe" session-scoped rules. See the
// implementation for `IsRuleSafe` for how a rule's safety is determined.
int GetUnsafeSessionRuleLimit();

// Returns the per-extension regex rules limit. This is enforced separately for
// static and dynamic rulesets.
int GetRegexRuleLimit();

// Returns the per-extension maximum amount of disabled static rules.
int GetDisabledStaticRuleLimit();

// Test helpers to override the various rule limits until the returned value is
// in scope.
using ScopedRuleLimitOverride = base::AutoReset<int>;
ScopedRuleLimitOverride CreateScopedStaticGuaranteedMinimumOverrideForTesting(
    int minimum);
ScopedRuleLimitOverride CreateScopedGlobalStaticRuleLimitOverrideForTesting(
    int limit);
ScopedRuleLimitOverride CreateScopedRegexRuleLimitOverrideForTesting(int limit);
ScopedRuleLimitOverride CreateScopedDynamicRuleLimitOverrideForTesting(
    int limit);
ScopedRuleLimitOverride CreateScopedUnsafeDynamicRuleLimitOverrideForTesting(
    int limit);
ScopedRuleLimitOverride CreateScopedSessionRuleLimitOverrideForTesting(
    int limit);
ScopedRuleLimitOverride CreateScopedUnsafeSessionRuleLimitOverrideForTesting(
    int limit);
ScopedRuleLimitOverride CreateScopedDisabledStaticRuleLimitOverrideForTesting(
    int limit);

// Helper to convert a flatbufffers::String to a string-like object with type T.
template <typename T>
T CreateString(const flatbuffers::String& str) {
  return T(str.c_str(), str.size());
}

// Returns the number of static rules enabled for the specified
// |composite_matcher|.
size_t GetEnabledStaticRuleCount(const CompositeMatcher* composite_matcher);

// Whether the `extension` has the permission to use the declarativeNetRequest
// API.
bool HasAnyDNRPermission(const Extension& extension);

// Returns true if |extension| has the declarativeNetRequestFeedback permission
// for the specified |tab_id|. If |tab_is| is omitted, then non-tab specific
// permissions are checked.
bool HasDNRFeedbackPermission(const Extension* extension,
                              const std::optional<int>& tab_id);

// Returns the appropriate error string for an unsuccessful rule parsing result.
std::string GetParseError(ParseResult error_reason, int rule_id);

// Maps resource types to flat_rule::ElementType.
url_pattern_index::flat::ElementType GetElementType(
    WebRequestResourceType web_request_type);
url_pattern_index::flat::ElementType GetElementType(
    api::declarative_net_request::ResourceType resource_type);

// Maps HTTP request methods to flat_rule::RequestMethod.
// Returns `flat::RequestMethod_NON_HTTP` for non-HTTP(s) requests.
url_pattern_index::flat::RequestMethod GetRequestMethod(
    bool http_or_https,
    const std::string& method);
url_pattern_index::flat::RequestMethod GetRequestMethod(
    api::declarative_net_request::RequestMethod request_method);
url_pattern_index::flat::RequestMethod GetRequestMethod(
    bool http_or_https,
    api::declarative_net_request::RequestMethod request_method);

bool IsRuleSafe(const api::declarative_net_request::Rule& rule);
bool IsRuleSafe(const flat::UrlRuleMetadata& rule);

// Returns if the browser has enabled matching by response header conditions.
// This looks at the `kDeclarativeNetRequestResponseHeaderMatching` feature flag
// and the current browser channel.
bool IsResponseHeaderMatchingEnabled();

// Returns if the browser has enabled regex substitutions (and filtering) for
// modifyHeaders rules.
bool IsHeaderSubstitutionEnabled();

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_UTILS_H_
