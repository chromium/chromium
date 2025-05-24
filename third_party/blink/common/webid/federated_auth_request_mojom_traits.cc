// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/webid/federated_auth_request_mojom_traits.h"

#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-shared.h"
#include "url/url_constants.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::LoginStatusAccountDataView,
                  blink::common::webid::LoginStatusAccount>::
    Read(blink::mojom::LoginStatusAccountDataView data,
         blink::common::webid::LoginStatusAccount* out) {
  bool read_succeeded = data.ReadId(&out->id) && data.ReadEmail(&out->email) &&
                        data.ReadName(&out->name) &&
                        data.ReadGivenName(&out->given_name) &&
                        data.ReadPicture(&out->picture);

  if (!read_succeeded) {
    return false;
  }

  // Validate that the picture URL, if supplied, is a valid potentially
  // trustworthy URL.
  if (!out->picture.has_value()) {
    return true;
  }

  return out->picture->is_valid() &&
         network::IsUrlPotentiallyTrustworthy(out->picture.value());
}

// static
bool StructTraits<blink::mojom::LoginStatusOptionsDataView,
                  blink::common::webid::LoginStatusOptions>::
    Read(blink::mojom::LoginStatusOptionsDataView data,
         blink::common::webid::LoginStatusOptions* out) {
  return data.ReadAccounts(&out->accounts) &&
         data.ReadExpiration(&out->expiration);
}

}  // namespace mojo
