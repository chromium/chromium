// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DEFAULT_CREDENTIALS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DEFAULT_CREDENTIALS_MOJOM_TRAITS_H_

#include "ipc/ipc_message_utils.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/http/http_auth_preferences.h"
#include "services/network/public/mojom/default_credentials.mojom.h"

namespace mojo {

template <>
struct EnumTraits<network::mojom::DefaultCredentials,
                  net::HttpAuthPreferences::DefaultCredentials> {
  static network::mojom::DefaultCredentials ToMojom(
      net::HttpAuthPreferences::DefaultCredentials input);

  static bool FromMojom(network::mojom::DefaultCredentials input,
                        net::HttpAuthPreferences::DefaultCredentials* output);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DEFAULT_CREDENTIALS_MOJOM_TRAITS_H_
