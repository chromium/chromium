// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_param_mojom_traits.h"

#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

bool StructTraits<network::mojom::AuthChallengeInfoDataView,
                  net::AuthChallengeInfo>::
    Read(network::mojom::AuthChallengeInfoDataView data,
         net::AuthChallengeInfo* out) {
  out->is_proxy = data.is_proxy();
  if (!data.ReadChallenger(&out->challenger) ||
      !data.ReadScheme(&out->scheme) || !data.ReadRealm(&out->realm) ||
      !data.ReadChallenge(&out->challenge) || !data.ReadPath(&out->path)) {
    return false;
  }
  return true;
}

bool StructTraits<network::mojom::HttpVersionDataView, net::HttpVersion>::Read(
    network::mojom::HttpVersionDataView data,
    net::HttpVersion* out) {
  *out = net::HttpVersion(data.major_value(), data.minor_value());
  return true;
}

}  // namespace mojo
