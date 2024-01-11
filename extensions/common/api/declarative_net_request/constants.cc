// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request/constants.h"

namespace extensions {
namespace declarative_net_request {

const char kDeclarativeNetRequestPermission[] = "declarativeNetRequest";
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
const char kInitiatorDomainsKey[] = "initiatorDomains";
const char kExcludedInitiatorDomainsKey[] = "excludedInitiatorDomains";
const char kRequestDomainsKey[] = "requestDomains";
const char kExcludedRequestDomainsKey[] = "excludedRequestDomains";
const char kResourceTypesKey[] = "resourceTypes";
const char kRequestMethodsKey[] = "requestMethods";
const char kExcludedRequestMethodsKey[] = "excludedRequestMethods";
const char kExcludedResourceTypesKey[] = "excludedResourceTypes";
const char kDomainTypeKey[] = "domainType";
const char kRuleActionTypeKey[] = "type";
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
const char kQueryReplaceOnlyKey[] = "replaceOnly";
const char kRegexSubstitutionKey[] = "regexSubstitution";
const char kRegexSubstitutionPath[] = "action.redirect.regexSubstitution";
const char kRequestHeadersKey[] = "requestHeaders";
const char kResponseHeadersKey[] = "responseHeaders";
const char kModifyRequestHeadersPath[] = "action.requestHeaders";
const char kModifyResponseHeadersPath[] = "action.responseHeaders";
const char kHeaderNameKey[] = "header";
const char kHeaderOperationKey[] = "operation";
const char kHeaderValueKey[] = "value";
const char kTabIdsKey[] = "tabIds";
const char kExcludedTabIdsKey[] = "excludedTabIds";

const char kMatchResponseHeadersPath[] = "condition.responseHeaders";
const char kMatchExcludedResponseHeadersPath[] =
    "condition.excludedResponseHeaders";
const char kHeaderValuesKey[] = "values";
const char kHeaderExcludedValuesKey[] = "excludedValues";
const char kExcludedResponseHeadersKey[] = "excludedResponseHeaders";

}  // namespace declarative_net_request
}  // namespace extensions
