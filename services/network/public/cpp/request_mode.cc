// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/request_mode.h"

namespace network {

const char* RequestModeToString(network::mojom::RequestMode mode) {
  switch (mode) {
    case network::mojom::RequestMode::kSameOrigin:
      return "same-origin";
    case network::mojom::RequestMode::kNoCors:
      return "no-cors";
    case network::mojom::RequestMode::kCors:
    case network::mojom::RequestMode::kCorsWithForcedPreflight:
      return "cors";
    case network::mojom::RequestMode::kNavigate:
      return "navigate";
  }
  NOTREACHED();
  return "";
}

}  // namespace network
