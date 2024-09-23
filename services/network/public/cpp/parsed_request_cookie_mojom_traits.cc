// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/parsed_request_cookie_mojom_traits.h"

#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/parsed_request_cookie.mojom-shared.h"

namespace mojo {

// static
bool StructTraits<network::mojom::ParsedRequestCookieDataView,
                  net::cookie_util::ParsedRequestCookie>::
    Read(network::mojom::ParsedRequestCookieDataView data,
         net::cookie_util::ParsedRequestCookie* out) {
  return data.ReadName(&out->first) && data.ReadValue(&out->second);
}

}  // namespace mojo
