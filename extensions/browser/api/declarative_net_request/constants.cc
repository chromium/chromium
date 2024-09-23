// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/constants.h"

#include "extensions/common/constants.h"
#include "url/url_constants.h"

namespace extensions::declarative_net_request {

const char* const kAllowedTransformSchemes[4] = {
    url::kHttpScheme, url::kHttpsScheme, url::kFtpScheme,
    extensions::kExtensionScheme};

const char kErrorRequestMethodDuplicated[] =
    "Rule with id * includes and excludes the same request method.";
const char kErrorResourceTypeDuplicated[] =
    "Rule with id * includes and excludes the same resource.";
const char kErrorInvalidRuleKey[] =
    "Rule with id * has an invalid value for * key. This should be greater "
    "than or equal to *.";
const char kErrorNoApplicableResourceTypes[] =
    "Rule with id * is not applicable to any resource type.";
const char kErrorEmptyList[] =
    "Rule with id * cannot have an empty list as the value for * key.";
const char kErrorEmptyKey[] =
    "Rule with id * cannot have an empty value for * key.";
const char kErrorInvalidRedirectUrl[] =
    "Rule with id * does not provide a valid URL for * key.";
const char kErrorDuplicateIDs[] = "Rule with id * does not have a unique ID.";
// Don't surface the actual error to the user, since it's an implementation
// detail.
const char kErrorPersisting[] = "Internal error while parsing rules.";
const char kErrorNonAscii[] =
    "Rule with id * cannot have non-ascii characters as part of \"*\" key.";
const char kErrorInvalidKey[] =
    "Rule with id * specifies an incorrect value for the \"*\" key.";
const char kErrorInvalidTransformScheme[] =
    "Rule with id * specifies an incorrect value for the \"*\" key. Allowed "
    "values are: [*].";
const char kErrorQueryAndTransformBothSpecified[] =
    "Rule with id * cannot specify both \"*\" and \"*\" keys.";
const char kErrorDomainsAndInitiatorDomainsBothSpecified[] =
    "Rule with id * cannot use deprecated field \"*\". Use \"*\" instead.";
const char kErrorJavascriptRedirect[] =
    "Rule with id * specifies an incorrect value for the \"*\" key. Redirects "
    "to javascript urls are not supported.";
const char kErrorMultipleFilters[] =
    "Rule with id * can only specify one of \"*\" or \"*\" keys.";
const char kErrorRegexSubstitutionWithoutFilter[] =
    "Rule with id * can't specify the \"*\" key without specifying the \"*\" "
    "key.";
const char kErrorInvalidAllowAllRequestsResourceType[] =
    "Rule with id * is an \"allowAllRequests\" rule and must specify the "
    "\"resourceTypes\" key. It may only include the \"main_frame\" and "
    "\"sub_frame\" resource types.";
const char kErrorRegexTooLarge[] =
    "Rule with id * was skipped as the \"*\" value exceeded the 2KB memory "
    "limit when compiled. Learn more: "
    "https://developer.chrome.com/docs/extensions/reference/api/"
    "declarativeNetRequest#regex-rules";
const char kErrorNoHeaderListsSpecified[] =
    "Rule with id * does not specify a value for \"*\" or \"*\" key. At least "
    "one of these keys must be specified with a non-empty list.";
const char kErrorInvalidModifyHeaderName[] =
    "Rule with id * must specify a valid header name to be modified.";
const char kErrorInvalidModifyHeaderValue[] =
    "Rule with id * must provide a valid header value to be appended/set.";
const char kErrorNoHeaderValueSpecified[] =
    "Rule with id * must provide a value for a header to be appended/set.";
const char kErrorHeaderValuePresent[] =
    "Rule with id * must not provide a header value for a header to be "
    "removed.";
const char kErrorAppendInvalidRequestHeader[] =
    "Rule with id * specifies an invalid request header to be appended. Only "
    "standard HTTP request headers that can specify multiple values for a "
    "single entry are supported.";
const char kErrorTabIdsOnNonSessionRule[] =
    "Rule with id * specifies a value for \"*\" or \"*\" key. These are only "
    "supported for session-scoped rules.";
const char kErrorTabIdDuplicated[] =
    "Rule with id * includes and excludes the same tab ID.";
const char kErrorInvalidMatchingHeaderName[] =
    "Rule with id * must specify a valid header name for \"*\" key";
const char kErrorInvalidMatchingHeaderValue[] =
    "Rule with id * must specify a valid header value for \"*\" key";
const char kErrorResponseHeaderDuplicated[] =
    "Rule with id * includes and excludes the same response header.";
const char kErrorResponseHeaderRuleCannotModifyRequestHeaders[] =
    "Rule with id * which matches on response headers cannot modify request "
    "headers.";

const char kErrorListNotPassed[] = "Rules file must contain a list.";

const char kRuleCountExceeded[] =
    "Rule count exceeded. Some rules were ignored.";
const char kRegexRuleCountExceeded[] =
    "Regular expression rule count exceeded. Some rules were ignored.";
const char kEnabledRuleCountExceeded[] =
    "The number of enabled rules exceeds the API limits. Some rulesets will be "
    "ignored.";
const char kEnabledRegexRuleCountExceeded[] =
    "The number of enabled regular expression rules exceeds the API limits. "
    "Some rulesets will be ignored.";
const char kRuleNotParsedWarning[] =
    "Rule with * couldn't be parsed. Parse error: *.";
const char kTooManyParseFailuresWarning[] =
    "Too many rule parse failures; Reporting the first *.";
const char kIndexingRuleLimitExceeded[] =
    "Ruleset with id * exceeds the indexing rule limit and will be ignored.";
const char kInternalErrorUpdatingDynamicRules[] =
    "Internal error while updating dynamic rules.";
const char kInternalErrorGettingDynamicRules[] =
    "Internal error while getting dynamic rules.";
const char kDynamicRuleCountExceeded[] = "Dynamic rule count exceeded.";

// TODO(crbug.com/40282671): Once the documentation is updated, add a link to
// the page detailing what safe/unsafe rules are.
const char kDynamicUnsafeRuleCountExceeded[] =
    "Dynamic unsafe rule count exceeded.";
const char kDynamicRegexRuleCountExceeded[] =
    "Dynamic rule count for regex rules exceeded.";

const char kSessionRuleCountExceeded[] = "Session rule count exceeded.";

// TODO(crbug.com/40282671): Once the documentation is updated, add a link to
// the page detailing what safe/unsafe rules are.
const char kSessionUnsafeRuleCountExceeded[] =
    "Session unsafe rule count exceeded.";
const char kSessionRegexRuleCountExceeded[] =
    "Session rule count for regex rules exceeded.";

const char kInvalidRulesetIDError[] = "Invalid ruleset id: *.";
const char kEnabledRulesetsRuleCountExceeded[] =
    "The set of enabled rulesets exceeds the rule count limit.";
const char kEnabledRulesetsRegexRuleCountExceeded[] =
    "The set of enabled rulesets exceeds the regular expression rule count "
    "limit.";
const char kInternalErrorUpdatingEnabledRulesets[] = "Internal error.";
const char kEnabledRulesetCountExceeded[] =
    "The number of enabled static rulesets exceeds the enabled ruleset count "
    "limit.";

const char kDisabledStaticRuleCountExceeded[] =
    "The number of disabled static rules exceeds the disabled rule count "
    "limit.";

const char kTabNotFoundError[] = "No tab with id: *.";
const char kIncrementActionCountWithoutUseAsBadgeTextError[] =
    "Cannot increment action count unless displaying action count as badge "
    "text.";

const char kInvalidTestURLError[] = "Invalid test request URL.";
const char kInvalidTestInitiatorError[] = "Invalid test request initiator.";
const char kInvalidTestTabIdError[] = "Invalid test request tab ID.";
const char kInvalidResponseHeaderObjectError[] =
    R"(Values for header "*" must be specified as a list.)";
const char kInvalidResponseHeaderNameError[] = R"(Invalid header name "*".)";
const char kInvalidResponseHeaderValueError[] =
    R"(Invalid header value for header "*".)";

const char kIndexAndPersistRulesTimeHistogram[] =
    "Extensions.DeclarativeNetRequest.IndexAndPersistRulesTime";
const char kManifestEnabledRulesCountHistogram[] =
    "Extensions.DeclarativeNetRequest.ManifestEnabledRulesCount2";
const char kUpdateDynamicRulesStatusHistogram[] =
    "Extensions.DeclarativeNetRequest.UpdateDynamicRulesStatus";
const char kReadDynamicRulesJSONStatusHistogram[] =
    "Extensions.DeclarativeNetRequest.ReadDynamicRulesJSONStatus";
const char kIsLargeRegexHistogram[] =
    "Extensions.DeclarativeNetRequest.IsLargeRegexRule";
const char kRegexRuleSizeHistogram[] =
    "Extensions.DeclarativeNetRequest.RegexRuleSize";
const char kLoadRulesetResultHistogram[] =
    "Extensions.DeclarativeNetRequest.LoadRulesetResult";

const char kActionCountPlaceholderBadgeText[] =
    "<<declarativeNetRequestActionCount>>";

const char kErrorGetMatchedRulesMissingPermissions[] =
    "The extension must have the declarativeNetRequestFeedback permission or "
    "have activeTab granted for the specified tab ID in order to call this "
    "function.";

const char kEmbedderConditionsBufferIdentifier[] = "EMBR";

}  // namespace extensions::declarative_net_request
