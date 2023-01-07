// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"

namespace extensions {
namespace declarative_webrequest_constants {

// Signals to which WebRequestRulesRegistries are registered.
const char kOnRequest[] = "declarativeWebRequest.onRequest";
const char kOnMessage[] = "declarativeWebRequest.onMessage";

// Keys of dictionaries.
const char kAgeLowerBoundKey[] = "ageLowerBound";
const char kAgeUpperBoundKey[] = "ageUpperBound";
const char kCookieKey[] = "cookie";
const char kContentTypeKey[] = "contentType";
const char kDeprecatedFirstPartyForCookiesUrlKey[] = "firstPartyForCookiesUrl";
const char kDeprecatedThirdPartyKey[] = "thirdPartyForCookies";
const char kDomainKey[] = "domain";
const char kExcludeContentTypeKey[] = "excludeContentType";
const char kExcludeRequestHeadersKey[] = "excludeRequestHeaders";
const char kExcludeResponseHeadersKey[] = "excludeResponseHeaders";
const char kExpiresKey[] = "expires";
const char kFilterKey[] ="filter";
const char kFromKey[] = "from";
const char kHttpOnlyKey[] = "httpOnly";
const char kHasTagKey[] = "hasTag";
const char kInstanceTypeKey[] = "instanceType";
const char kLowerPriorityThanKey[] = "lowerPriorityThan";
const char kMaxAgeKey[] = "maxAge";
const char kMessageKey[] = "message";
const char kModificationKey[] = "modification";
const char kNameContainsKey[] = "nameContains";
const char kNameEqualsKey[] = "nameEquals";
const char kNameKey[] = "name";
const char kNamePrefixKey[] = "namePrefix";
const char kNameSuffixKey[] = "nameSuffix";
const char kPathKey[] = "path";
const char kRedirectUrlKey[] = "redirectUrl";
const char kRequestHeadersKey[] = "requestHeaders";
const char kResourceTypeKey[] = "resourceType";
const char kResponseHeadersKey[] = "responseHeaders";
const char kSecureKey[] = "secure";
const char kSessionCookieKey[] = "sessionCookie";
const char kStagesKey[] = "stages";
const char kToKey[] = "to";
const char kUrlKey[] = "url";
const char kValueContainsKey[] = "valueContains";
const char kValueEqualsKey[] = "valueEquals";
const char kValueKey[] = "value";
const char kValuePrefixKey[] = "valuePrefix";
const char kValueSuffixKey[] = "valueSuffix";

// Enum string values
const char kOnBeforeRequestEnum[] = "onBeforeRequest";
const char kOnBeforeSendHeadersEnum[] = "onBeforeSendHeaders";
const char kOnHeadersReceivedEnum[] = "onHeadersReceived";
const char kOnAuthRequiredEnum[] = "onAuthRequired";

// Values of dictionaries, in particular instance types
const char kAddRequestCookieType[] = "declarativeWebRequest.AddRequestCookie";
const char kAddResponseCookieType[] = "declarativeWebRequest.AddResponseCookie";
const char kAddResponseHeaderType[] = "declarativeWebRequest.AddResponseHeader";
const char kCancelRequestType[] = "declarativeWebRequest.CancelRequest";
const char kEditRequestCookieType[] = "declarativeWebRequest.EditRequestCookie";
const char kEditResponseCookieType[] =
    "declarativeWebRequest.EditResponseCookie";
const char kIgnoreRulesType[] = "declarativeWebRequest.IgnoreRules";
const char kRedirectRequestType[] = "declarativeWebRequest.RedirectRequest";
const char kRedirectByRegExType[] =
    "declarativeWebRequest.RedirectByRegEx";
const char kRedirectToEmptyDocumentType[] =
    "declarativeWebRequest.RedirectToEmptyDocument";
const char kRedirectToTransparentImageType[] =
    "declarativeWebRequest.RedirectToTransparentImage";
const char kRemoveRequestCookieType[] =
    "declarativeWebRequest.RemoveRequestCookie";
const char kRemoveRequestHeaderType[] =
    "declarativeWebRequest.RemoveRequestHeader";
const char kRemoveResponseCookieType[] =
    "declarativeWebRequest.RemoveResponseCookie";
const char kRemoveResponseHeaderType[] =
    "declarativeWebRequest.RemoveResponseHeader";
const char kRequestMatcherType[] = "declarativeWebRequest.RequestMatcher";
const char kSendMessageToExtensionType[] =
    "declarativeWebRequest.SendMessageToExtension";
const char kSetRequestHeaderType[] = "declarativeWebRequest.SetRequestHeader";

}  // namespace declarative_webrequest_constants
}  // namespace extensions
