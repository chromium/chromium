// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/constants.h"

#include "extensions/common/constants.h"
#include "url/url_constants.h"

namespace extensions {
namespace declarative_net_request {

const char* const kAllowedTransformSchemes[4] = {
    url::kHttpScheme, url::kHttpsScheme, url::kFtpScheme,
    extensions::kExtensionScheme};

const char kErrorResourceTypeDuplicated[] =
    "Rule with id * includes and excludes the same resource.";
const char kErrorEmptyRedirectRuleKey[] =
    "Rule with id * does not specify the value for * key. This is required "
    "for redirect rules.";
const char kErrorEmptyUpgradeRulePriority[] =
    "Rule with id * does not specify the value for priority key. This is "
    "required for upgradeScheme rules.";
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
const char kErrorEmptyRemoveHeadersList[] =
    "Rule with id * does not specify the value for \"*\" key. This is required "
    "for \"removeHeaders\" rules.";
const char kErrorInvalidKey[] =
    "Rule with id * specifies an incorrect value for the \"*\" key.";
const char kErrorInvalidTransformScheme[] =
    "Rule with id * specifies an incorrect value for the \"*\" key. Allowed "
    "values are: [*].";
const char kErrorQueryAndTransformBothSpecified[] =
    "Rule with id * cannot specify both \"*\" and \"*\" keys.";
const char kErrorJavascriptRedirect[] =
    "Rule with id * specifies an incorrect value for the \"*\" key. Redirects "
    "to javascript urls are not supported.";
const char kErrorMultipleFilters[] =
    "Rule with id * can only specify one of \"*\" or \"*\" keys.";

const char kErrorListNotPassed[] = "Rules file must contain a list.";

const char kRuleCountExceeded[] =
    "Declarative Net Request: Rule count exceeded. Some rules were ignored.";
const char kRuleNotParsedWarning[] =
    "Declarative Net Request: Rule with * couldn't be parsed. Parse error: "
    "*.";
const char kTooManyParseFailuresWarning[] =
    "Declarative Net Request: Too many rule parse failures; Reporting the "
    "first *.";
const char kInternalErrorUpdatingDynamicRules[] =
    "Internal error while updating dynamic rules.";
const char kInternalErrorGettingDynamicRules[] =
    "Internal error while getting dynamic rules.";
const char kDynamicRuleCountExceeded[] = "Dynamic rule count exceeded.";
const char kIndexAndPersistRulesTimeHistogram[] =
    "Extensions.DeclarativeNetRequest.IndexAndPersistRulesTime";
const char kManifestRulesCountHistogram[] =
    "Extensions.DeclarativeNetRequest.ManifestRulesCount";
const char kUpdateDynamicRulesStatusHistogram[] =
    "Extensions.DeclarativeNetRequest.UpdateDynamicRulesStatus";
const char kReadDynamicRulesJSONStatusHistogram[] =
    "Extensions.DeclarativeNetRequest.ReadDynamicRulesJSONStatus";

const char kActionCountPlaceholderBadgeText[] =
    "<<declarativeNetRequestActionCount>>";

}  // namespace declarative_net_request
}  // namespace extensions
