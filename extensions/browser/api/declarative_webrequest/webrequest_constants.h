// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WebRequest API.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONSTANTS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONSTANTS_H_

namespace extensions {
namespace declarative_webrequest_constants {

// Signals to which WebRequestRulesRegistries are registered or listeners can
// be registered.
extern const char kOnRequest[];
extern const char kOnMessage[];

// Keys of dictionaries.
extern const char kAgeLowerBoundKey[];
extern const char kAgeUpperBoundKey[];
extern const char kCookieKey[];
extern const char kContentTypeKey[];
extern const char kDeprecatedFirstPartyForCookiesUrlKey[];
extern const char kDeprecatedThirdPartyKey[];
extern const char kDomainKey[];
extern const char kExcludeContentTypeKey[];
extern const char kExcludeRequestHeadersKey[];
extern const char kExcludeResponseHeadersKey[];
extern const char kExpiresKey[];
extern const char kFilterKey[];
extern const char kFromKey[];
extern const char kHttpOnlyKey[];
extern const char kHasTagKey[];
extern const char kInstanceTypeKey[];
extern const char kLowerPriorityThanKey[];
extern const char kMaxAgeKey[];
extern const char kMessageKey[];
extern const char kModificationKey[];
extern const char kNameContainsKey[];
extern const char kNameEqualsKey[];
extern const char kNameKey[];
extern const char kNamePrefixKey[];
extern const char kNameSuffixKey[];
extern const char kPathKey[];
extern const char kRedirectUrlKey[];
extern const char kRequestHeadersKey[];
extern const char kResourceTypeKey[];
extern const char kResponseHeadersKey[];
extern const char kSecureKey[];
extern const char kSessionCookieKey[];
extern const char kStagesKey[];
extern const char kToKey[];
extern const char kUrlKey[];
extern const char kValueContainsKey[];
extern const char kValueEqualsKey[];
extern const char kValueKey[];
extern const char kValuePrefixKey[];
extern const char kValueSuffixKey[];

// Enum string values
extern const char kOnBeforeRequestEnum[];
extern const char kOnBeforeSendHeadersEnum[];
extern const char kOnHeadersReceivedEnum[];
extern const char kOnAuthRequiredEnum[];

// Values of dictionaries, in particular instance types
extern const char kAddRequestCookieType[];
extern const char kAddResponseCookieType[];
extern const char kAddResponseHeaderType[];
extern const char kCancelRequestType[];
extern const char kEditRequestCookieType[];
extern const char kEditResponseCookieType[];
extern const char kIgnoreRulesType[];
extern const char kRedirectByRegExType[];
extern const char kRedirectRequestType[];
extern const char kRedirectToEmptyDocumentType[];
extern const char kRedirectToTransparentImageType[];
extern const char kRemoveRequestCookieType[];
extern const char kRemoveRequestHeaderType[];
extern const char kRemoveResponseCookieType[];
extern const char kRemoveResponseHeaderType[];
extern const char kRequestMatcherType[];
extern const char kSendMessageToExtensionType[];
extern const char kSetRequestHeaderType[];

}  // namespace declarative_webrequest_constants
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_WEBREQUEST_CONSTANTS_H_
