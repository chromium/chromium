// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_CONSTANTS_H_
#define EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_CONSTANTS_H_

#include "base/types/id_type.h"

namespace extensions::declarative_net_request {

// Permission name.
extern const char kDeclarativeNetRequestPermission[];

// Feedback permission name.
extern const char kFeedbackAPIPermission[];

// Minimum valid value of a declarative rule ID.
inline constexpr int kMinValidID = 1;

// Minimum valid value of a declarative rule priority.
inline constexpr int kMinValidPriority = 1;

using RulesetID =
    ::base::IdType<class RulesetIDTag, int, -2 /* invalid value */>;

inline constexpr RulesetID kMinValidStaticRulesetID(1);
inline constexpr RulesetID kDynamicRulesetID(0);
inline constexpr RulesetID kSessionRulesetID(-1);

// Prefix for reserved ruleset public IDs. Extensions can't specify static
// rulesets beginning with this.
inline constexpr char kReservedRulesetIDPrefix = '_';

// Default priority used for rules when the priority is not explicity provided
// by an extension.
inline constexpr int kDefaultPriority = 1;

// Keys used in rules.
extern const char kIDKey[];
extern const char kPriorityKey[];
extern const char kRuleConditionKey[];
extern const char kRuleActionKey[];
extern const char kUrlFilterKey[];
extern const char kRegexFilterKey[];
extern const char kIsUrlFilterCaseSensitiveKey[];
extern const char kDomainsKey[];
extern const char kExcludedDomainsKey[];
extern const char kInitiatorDomainsKey[];
extern const char kExcludedInitiatorDomainsKey[];
extern const char kRequestDomainsKey[];
extern const char kExcludedRequestDomainsKey[];
extern const char kResourceTypesKey[];
extern const char kExcludedResourceTypesKey[];
extern const char kRequestMethodsKey[];
extern const char kExcludedRequestMethodsKey[];
extern const char kDomainTypeKey[];
extern const char kRuleActionTypeKey[];
extern const char kRedirectPath[];
extern const char kExtensionPathPath[];
extern const char kTransformSchemePath[];
extern const char kTransformPortPath[];
extern const char kTransformQueryPath[];
extern const char kTransformFragmentPath[];
extern const char kTransformQueryTransformPath[];
extern const char kRedirectKey[];
extern const char kExtensionPathKey[];
extern const char kRedirectUrlKey[];
extern const char kRedirectUrlPath[];
extern const char kTransformKey[];
extern const char kTransformSchemeKey[];
extern const char kTransformHostKey[];
extern const char kTransformPortKey[];
extern const char kTransformPathKey[];
extern const char kTransformQueryKey[];
extern const char kTransformQueryTransformKey[];
extern const char kTransformFragmentKey[];
extern const char kTransformUsernameKey[];
extern const char kTransformPasswordKey[];
extern const char kQueryTransformRemoveParamsKey[];
extern const char kQueryTransformAddReplaceParamsKey[];
extern const char kQueryKeyKey[];
extern const char kQueryValueKey[];
extern const char kQueryReplaceOnlyKey[];
extern const char kRegexSubstitutionKey[];
extern const char kRegexSubstitutionPath[];
extern const char kRequestHeadersKey[];
extern const char kResponseHeadersKey[];
extern const char kModifyRequestHeadersPath[];
extern const char kModifyResponseHeadersPath[];
extern const char kHeaderNameKey[];
extern const char kHeaderOperationKey[];
extern const char kHeaderValueKey[];
extern const char kTabIdsKey[];
extern const char kExcludedTabIdsKey[];
extern const char kMatchResponseHeadersPath[];
extern const char kMatchExcludedResponseHeadersPath[];
extern const char kHeaderValuesKey[];
extern const char kHeaderExcludedValuesKey[];
extern const char kExcludedResponseHeadersKey[];

}  // namespace extensions::declarative_net_request

#endif  // EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_CONSTANTS_H_
