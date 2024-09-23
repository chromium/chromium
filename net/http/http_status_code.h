// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STATUS_CODE_H_
#define NET_HTTP_HTTP_STATUS_CODE_H_

#include <optional>

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
// field in an HTTP response for given |code|. It's based on the IANA HTTP
// Status Code Registry.
// http://www.iana.org/assignments/http-status-codes/http-status-codes.xml
//
// This function may not cover all codes defined in the IANA registry. It
// returns an empty string (or crash in debug build) for status codes which are
// not yet covered or just invalid. Please extend it when needed.
NET_EXPORT const char* GetHttpReasonPhrase(HttpStatusCode code);

// Similar to the one above, but returns a nullptr in case code does not map
// to a known reasone phrase, which allows a gentle recovery in case the code
// was obtained from a non-trusted party.
NET_EXPORT const char* TryToGetHttpReasonPhrase(HttpStatusCode code);

// Returns the corresponding HTTP status code enum value for a given
// |response_code|. Returns std::nullopt if the status code is not in the IANA
// HTTP Status Code Registry.
NET_EXPORT const std::optional<HttpStatusCode> TryToGetHttpStatusCode(
    int response_code);

}  // namespace net

#endif  // NET_HTTP_HTTP_STATUS_CODE_H_
