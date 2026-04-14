// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/auth_mojom_traits.h"

#include <string>

#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "url/mojom/scheme_host_port_mojom_traits.h"

namespace mojo {

// static
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

// static
bool StructTraits<
    network::mojom::AuthCredentialsDataView,
    net::AuthCredentials>::Read(network::mojom::AuthCredentialsDataView data,
                                net::AuthCredentials* out) {
  std::u16string username;
  std::u16string password;
  if (!data.ReadUsername(&username) || !data.ReadPassword(&password)) {
    return false;
  }
  out->Set(username, password);
  return true;
}

}  // namespace mojo
