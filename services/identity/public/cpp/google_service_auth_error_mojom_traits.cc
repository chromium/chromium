// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/google_service_auth_error_mojom_traits.h"

#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<identity::mojom::GoogleServiceAuthError::DataView,
                  ::GoogleServiceAuthError>::
    Read(identity::mojom::GoogleServiceAuthErrorDataView data,
         ::GoogleServiceAuthError* out) {
  auto state = ::GoogleServiceAuthError::State(data.state());
  std::string error_message;
  if (!::GoogleServiceAuthError::IsValid(state) ||
      !data.ReadErrorMessage(&error_message)) {
    return false;
  }

  out->state_ = state;
  out->error_message_ = error_message;
  out->network_error_ = data.network_error();

  return true;
}

}  // namespace mojo
