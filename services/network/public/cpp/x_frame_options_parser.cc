// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/x_frame_options_parser.h"

#include <string>
#include <string_view>

#include "base/strings/string_util.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/x_frame_options.mojom-shared.h"

namespace network {

mojom::XFrameOptionsValue ParseXFrameOptions(
    const net::HttpResponseHeaders& headers) {
  // Process the 'X-Frame-Options' header:
  // https://html.spec.whatwg.org/multipage/browsing-the-web.html#the-x-frame-options-header
  //
  // Note that we do not support the 'ALLOW-FROM' value defined in RFC7034.
  mojom::XFrameOptionsValue result = mojom::XFrameOptionsValue::kNone;
  size_t iter = 0;
  std::string value;
  while (headers.EnumerateHeader(&iter, "x-frame-options", &value)) {
    mojom::XFrameOptionsValue current = mojom::XFrameOptionsValue::kInvalid;

    std::string_view trimmed = base::TrimWhitespaceASCII(value, base::TRIM_ALL);

    if (base::EqualsCaseInsensitiveASCII(trimmed, "deny"))
      current = mojom::XFrameOptionsValue::kDeny;
    else if (base::EqualsCaseInsensitiveASCII(trimmed, "allowall"))
      current = mojom::XFrameOptionsValue::kAllowAll;
    else if (base::EqualsCaseInsensitiveASCII(trimmed, "sameorigin"))
      current = mojom::XFrameOptionsValue::kSameOrigin;

    if (result == mojom::XFrameOptionsValue::kNone)
      result = current;
    else if (result != current)
      result = mojom::XFrameOptionsValue::kConflict;
  }

  return result;
}

}  // namespace network
