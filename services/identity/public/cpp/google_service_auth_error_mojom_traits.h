// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_GOOGLE_SERVICE_AUTH_ERROR_MOJOM_TRAITS_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_GOOGLE_SERVICE_AUTH_ERROR_MOJOM_TRAITS_H_

#include <string>

#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/mojom/google_service_auth_error.mojom.h"

namespace mojo {

template <>
struct StructTraits<identity::mojom::GoogleServiceAuthError::DataView,
                    ::GoogleServiceAuthError> {
  static int state(const ::GoogleServiceAuthError& r) { return r.state(); }

  static int network_error(const ::GoogleServiceAuthError& r) {
    return r.network_error();
  }

  static const std::string& error_message(const ::GoogleServiceAuthError& r) {
    return r.error_message();
  }

  static bool Read(identity::mojom::GoogleServiceAuthErrorDataView data,
                   ::GoogleServiceAuthError* out);
};

}  // namespace mojo

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_GOOGLE_SERVICE_AUTH_ERROR_MOJOM_TRAITS_H_
