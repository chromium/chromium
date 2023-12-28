// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_RESOURCE_TYPE_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_RESOURCE_TYPE_H_

#include <stdint.h>

#include <string_view>

#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace extensions {

// Enumerates all resource/request types that WebRequest API cares about.
enum class WebRequestResourceType : uint8_t {
  MAIN_FRAME,
  SUB_FRAME,
  STYLESHEET,
  SCRIPT,
  IMAGE,
  FONT,
  OBJECT,
  XHR,
  PING,
  CSP_REPORT,
  MEDIA,
  WEB_SOCKET,
  WEB_TRANSPORT,
  WEBBUNDLE,

  OTHER,  // The type is unknown, or differs from all the above.
};

WebRequestResourceType ToWebRequestResourceType(
    const network::ResourceRequest& request,
    bool is_download);

// Returns a string representation of |type|.
const char* WebRequestResourceTypeToString(WebRequestResourceType type);

// Finds a |type| such that its string representation equals to |text|. Returns
// true iff the type is found.
bool ParseWebRequestResourceType(std::string_view text,
                                 WebRequestResourceType* type);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_RESOURCE_TYPE_H_
