// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_api_constants.h"

namespace extension_web_request_api_constants {

const char kChallengerKey[] = "challenger";
const char kDocumentIdKey[] = "documentId";
const char kDocumentLifecycleKey[] = "documentLifecycle";
const char kErrorKey[] = "error";
const char kFrameIdKey[] = "frameId";
const char kFrameTypeKey[] = "frameType";
const char kParentDocumentIdKey[] = "parentDocumentId";
const char kParentFrameIdKey[] = "parentFrameId";
const char kProcessIdKey[] = "processId";
const char kFromCache[] = "fromCache";
const char kHostKey[] = "host";
const char kIpKey[] = "ip";
const char kPortKey[] = "port";
const char kMethodKey[] = "method";
const char kRedirectUrlKey[] = "redirectUrl";
const char kRequestIdKey[] = "requestId";
const char kStatusCodeKey[] = "statusCode";
const char kStatusLineKey[] = "statusLine";
const char kTabIdKey[] = "tabId";
const char kTimeStampKey[] = "timeStamp";
const char kTypeKey[] = "type";
const char kUrlKey[] = "url";
const char kRequestBodyKey[] = "requestBody";
const char kRequestBodyErrorKey[] = "error";
const char kRequestBodyFormDataKey[] = "formData";
const char kRequestBodyRawKey[] = "raw";
const char kRequestBodyRawBytesKey[] = "bytes";
const char kRequestBodyRawFileKey[] = "file";
const char kRequestHeadersKey[] = "requestHeaders";
const char kResponseHeadersKey[] = "responseHeaders";
const char kHeaderNameKey[] = "name";
const char kHeaderValueKey[] = "value";
const char kHeaderBinaryValueKey[] = "binaryValue";
const char kIsProxyKey[] = "isProxy";
const char kMessageKey[] = "message";
const char kSchemeKey[] = "scheme";
const char kStageKey[] = "stage";
const char kRealmKey[] = "realm";
const char kAuthCredentialsKey[] = "authCredentials";
const char kUsernameKey[] = "username";
const char kPasswordKey[] = "password";
const char kInitiatorKey[] = "initiator";

const char kOnBeforeRedirectEvent[] = "webRequest.onBeforeRedirect";
const char kOnBeforeSendHeadersEvent[] = "webRequest.onBeforeSendHeaders";
const char kOnCompletedEvent[] = "webRequest.onCompleted";
const char kOnHeadersReceivedEvent[] = "webRequest.onHeadersReceived";
const char kOnResponseStartedEvent[] = "webRequest.onResponseStarted";
const char kOnSendHeadersEvent[] = "webRequest.onSendHeaders";
const char kOnAuthRequiredEvent[] = "webRequest.onAuthRequired";

const char kInvalidRedirectUrl[] = "redirectUrl '*' is not a valid URL.";
const char kInvalidBlockingResponse[] =
    "cancel cannot be true in the presence of other keys.";
const char kInvalidRequestFilterUrl[] = "'*' is not a valid URL pattern.";
const char kBlockingPermissionRequired[] =
    "You do not have permission to use blocking webRequest listeners. "
    "Be sure to declare the webRequestBlocking permission in your "
    "manifest. Note that webRequestBlocking is only allowed for extensions "
    "that are installed using ExtensionInstallForcelist.";
const char kHostPermissionsRequired[] =
    "You need to request host permissions in the manifest file in order to "
    "be notified about requests from the webRequest API.";
const char kInvalidHeaderKeyCombination[] =
    "requestHeaders and responseHeaders cannot both be present.";
const char kInvalidHeader[] = "Invalid header specification '*'.";
const char kInvalidHeaderName[] = "Invalid header name.";
const char kInvalidHeaderValue[] = "Header '*' has an invalid value.";

}  // namespace extension_web_request_api_constants
