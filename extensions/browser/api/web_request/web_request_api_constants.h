// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WebRequest API.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_CONSTANTS_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_CONSTANTS_H_

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extension_web_request_api_constants {

// Keys.
extern const char kChallengerKey[];
extern const char kDocumentIdKey[];
extern const char kDocumentLifecycleKey[];
extern const char kErrorKey[];
extern const char kFrameIdKey[];
extern const char kFrameTypeKey[];
extern const char kParentDocumentIdKey[];
extern const char kParentFrameIdKey[];
extern const char kProcessIdKey[];
extern const char kFromCache[];
extern const char kHostKey[];
extern const char kIpKey[];
extern const char kMethodKey[];
extern const char kPortKey[];
extern const char kRedirectUrlKey[];
extern const char kRequestIdKey[];
extern const char kStatusCodeKey[];
extern const char kStatusLineKey[];
extern const char kTabIdKey[];
extern const char kTimeStampKey[];
extern const char kTypeKey[];
extern const char kUrlKey[];
extern const char kRequestBodyKey[];
extern const char kRequestBodyErrorKey[];
extern const char kRequestBodyFormDataKey[];
extern const char kRequestBodyRawKey[];
extern const char kRequestBodyRawBytesKey[];
extern const char kRequestBodyRawFileKey[];
extern const char kPostDataKey[];
extern const char kPostDataFormKey[];
extern const char kRequestHeadersKey[];
extern const char kResponseHeadersKey[];
extern const char kHeadersKey[];
extern const char kHeaderNameKey[];
extern const char kHeaderValueKey[];
extern const char kHeaderBinaryValueKey[];
extern const char kIsProxyKey[];
extern const char kMessageKey[];
extern const char kSchemeKey[];
extern const char kStageKey[];
extern const char kRealmKey[];
extern const char kAuthCredentialsKey[];
extern const char kUsernameKey[];
extern const char kPasswordKey[];
extern const char kInitiatorKey[];
extern const char kSecurityInfoKey[];
extern const char kCertificatesKey[];
extern const char kFingerprintKey[];
extern const char kRawDerKey[];
extern const char kSha256Key[];
extern const char kStateKey[];

// Events.
extern const char kOnAuthRequiredEvent[];
extern const char kOnBeforeRedirectEvent[];
extern const char kOnBeforeRequestEvent[];
extern const char kOnBeforeSendHeadersEvent[];
extern const char kOnCompletedEvent[];
extern const char kOnErrorOccurredEvent[];
extern const char kOnHeadersReceivedEvent[];
extern const char kOnResponseStartedEvent[];
extern const char kOnSendHeadersEvent[];

// Stages.
inline constexpr char kOnAuthRequired[] = "onAuthRequired";
inline constexpr char kOnBeforeRedirect[] = "onBeforeRedirect";
inline constexpr char kOnBeforeRequest[] = "onBeforeRequest";
inline constexpr char kOnBeforeSendHeaders[] = "onBeforeSendHeaders";
inline constexpr char kOnCompleted[] = "onCompleted";
inline constexpr char kOnErrorOccurred[] = "onErrorOccurred";
inline constexpr char kOnHeadersReceived[] = "onHeadersReceived";
inline constexpr char kOnResponseStarted[] = "onResponseStarted";
inline constexpr char kOnSendHeaders[] = "onSendHeaders";

// Error messages.
extern const char kInvalidRedirectUrl[];
extern const char kInvalidBlockingResponse[];
extern const char kInvalidRequestFilterUrl[];
extern const char kBlockingPermissionRequired[];
extern const char kHostPermissionsRequired[];
extern const char kInvalidHeaderKeyCombination[];
extern const char kInvalidHeader[];
extern const char kInvalidHeaderName[];
extern const char kInvalidHeaderValue[];
extern const char kSecurityInfoAPINotAvailable[];
extern const char kSecurityInfoFlagAbsentInExtensions[];
extern const char kSecurityInfoFlagAbsentInControlledFrame[];

}  // namespace extension_web_request_api_constants

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_API_CONSTANTS_H_
