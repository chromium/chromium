// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/timing_allow_origin_parser.h"

#include <string>
#include <utility>
#include <vector>

#include "net/http/http_util.h"

namespace network {

// https://fetch.spec.whatwg.org/#concept-tao-check
// https://w3c.github.io/resource-timing/#dfn-timing-allow-origin
mojom::TimingAllowOriginPtr ParseTimingAllowOrigin(const std::string& value) {
  if (value == "*") {
    return mojom::TimingAllowOrigin::NewAll(/*ignored=*/0);
  }

  // This does not simply use something like `base::SplitStringPiece()`, as
  // https://fetch.spec.whatwg.org/#concept-tao-check specifies that quoted
  // strings should be supported.
  net::HttpUtil::ValuesIterator v(value.begin(), value.end(), ',');
  std::vector<std::string> values;
  while (v.GetNext()) {
    if (v.value_piece() == "*") {
      return mojom::TimingAllowOrigin::NewAll(/*ignored=*/0);
    }
    values.push_back(v.value());
  }
  return mojom::TimingAllowOrigin::NewSerializedOrigins(std::move(values));
}

}  // namespace network
