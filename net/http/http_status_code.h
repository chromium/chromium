// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STATUS_CODE_H_
#define NET_HTTP_HTTP_STATUS_CODE_H_

#include <optional>
#include <string_view>

#include "net/base/net_export.h"

namespace net {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// HTTP status codes.
enum HttpStatusCode {

#define HTTP_STATUS_ENUM_VALUE(label, code, reason) HTTP_##label = code,
#include "net/http/http_status_code_list.h"
#undef HTTP_STATUS_ENUM_VALUE

};

// Returns the corresponding HTTP status description to use in the Reason-Phrase
// field in an HTTP response for given `code`. It's based on the IANA HTTP
// Status Code Registry.
// http://www.iana.org/assignments/http-status-codes/http-status-codes.xml
//
// `default_value` is what is returned in the case of unrecognized values. This
// function may not cover all codes defined in the IANA registry. Please extend
// it when needed.
NET_EXPORT std::string_view GetHttpReasonPhrase(
    HttpStatusCode code,
    std::string_view default_value = "Unknown Status Code");

// Overload of above method that takes ints instead of HttpStatusCode.
inline std::string_view GetHttpReasonPhrase(
    int code,
    std::string_view default_value = "Unknown Status Code") {
  return GetHttpReasonPhrase(static_cast<HttpStatusCode>(code), default_value);
}

// Returns the corresponding HTTP status code enum value for a given
// |response_code|. Returns std::nullopt if the status code is not in the IANA
// HTTP Status Code Registry.
NET_EXPORT const std::optional<HttpStatusCode> TryToGetHttpStatusCode(
    int response_code);

}  // namespace net

#endif  // NET_HTTP_HTTP_STATUS_CODE_H_
