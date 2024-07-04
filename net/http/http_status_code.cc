// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_status_code.h"

#include <ostream>

#include "base/notreached.h"

namespace net {

const char* GetHttpReasonPhrase(HttpStatusCode code) {
  if (const char* phrase = TryToGetHttpReasonPhrase(code)) {
    return phrase;
  }
  DUMP_WILL_BE_NOTREACHED() << "unknown HTTP status code " << code;
  return nullptr;
}

const char* TryToGetHttpReasonPhrase(HttpStatusCode code) {
  switch (code) {
#define HTTP_STATUS_ENUM_VALUE(label, code, reason) \
  case HTTP_##label:                                \
    return reason;
#include "net/http/http_status_code_list.h"
#undef HTTP_STATUS_ENUM_VALUE

    default:
      return nullptr;
  }
}

const std::optional<HttpStatusCode> TryToGetHttpStatusCode(int response_code) {
  switch (response_code) {
#define HTTP_STATUS_ENUM_VALUE(label, code, reason) \
  case code:                                        \
    return HTTP_##label;
#include "net/http/http_status_code_list.h"
#undef HTTP_STATUS_ENUM_VALUE

    default:
      return std::nullopt;
  }
}

}  // namespace net
