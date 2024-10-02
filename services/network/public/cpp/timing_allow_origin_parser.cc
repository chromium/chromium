// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/timing_allow_origin_parser.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/timing_allow_origin.mojom-forward.h"

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
  net::HttpUtil::ValuesIterator v(value, /*delimiter=*/',');
  std::vector<std::string> values;
  while (v.GetNext()) {
    if (v.value() == "*") {
      return mojom::TimingAllowOrigin::NewAll(/*ignored=*/0);
    }
    values.emplace_back(v.value());
  }
  return mojom::TimingAllowOrigin::NewSerializedOrigins(std::move(values));
}

// https://fetch.spec.whatwg.org/#concept-tao-check
bool TimingAllowOriginCheck(const mojom::TimingAllowOriginPtr& tao,
                            const url::Origin& origin) {
  return tao &&
         (tao->which() == mojom::TimingAllowOrigin::Tag::kAll ||
          ::base::Contains(tao->get_serialized_origins(), origin.Serialize()));
}

}  // namespace network
