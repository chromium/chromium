// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/constants.h"

namespace extensions {
namespace declarative_net_request {

const char kAPIPermission[] = "declarativeNetRequest";
const char kFeedbackAPIPermission[] = "declarativeNetRequestFeedback";

const char kIDKey[] = "id";
const char kPriorityKey[] = "priority";
const char kRuleConditionKey[] = "condition";
const char kRuleActionKey[] = "action";
const char kUrlFilterKey[] = "urlFilter";
const char kRegexFilterKey[] = "regexFilter";
const char kIsUrlFilterCaseSensitiveKey[] = "isUrlFilterCaseSensitive";
const char kDomainsKey[] = "domains";
const char kExcludedDomainsKey[] = "excludedDomains";
const char kResourceTypesKey[] = "resourceTypes";
const char kExcludedResourceTypesKey[] = "excludedResourceTypes";
const char kDomainTypeKey[] = "domainType";
const char kRuleActionTypeKey[] = "type";
const char kRemoveHeadersListKey[] = "removeHeadersList";
const char kRedirectPath[] = "action.redirect";
const char kExtensionPathPath[] = "action.redirect.extensionPath";
const char kTransformSchemePath[] = "action.redirect.transform.scheme";
const char kTransformPortPath[] = "action.redirect.transform.port";
const char kTransformQueryPath[] = "action.redirect.transform.query";
const char kTransformFragmentPath[] = "action.redirect.transform.fragment";
const char kTransformQueryTransformPath[] =
    "action.redirect.transform.queryTransform";
const char kRedirectKey[] = "redirect";
const char kExtensionPathKey[] = "extensionPath";
const char kRedirectUrlKey[] = "url";
const char kRedirectUrlPath[] = "action.redirect.url";
const char kTransformKey[] = "transform";
const char kTransformSchemeKey[] = "scheme";
const char kTransformHostKey[] = "host";
const char kTransformPortKey[] = "port";
const char kTransformPathKey[] = "path";
const char kTransformQueryKey[] = "query";
const char kTransformQueryTransformKey[] = "queryTransform";
const char kTransformFragmentKey[] = "fragment";
const char kTransformUsernameKey[] = "username";
const char kTransformPasswordKey[] = "password";
const char kQueryTransformRemoveParamsKey[] = "removeParams";
const char kQueryTransformAddReplaceParamsKey[] = "addOrReplaceParams";
const char kQueryKeyKey[] = "key";
const char kQueryValueKey[] = "value";

}  // namespace declarative_net_request
}  // namespace extensions
