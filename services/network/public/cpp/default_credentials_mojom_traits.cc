// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/default_credentials_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

network::mojom::DefaultCredentials
EnumTraits<network::mojom::DefaultCredentials,
           net::HttpAuthPreferences::DefaultCredentials>::
    ToMojom(net::HttpAuthPreferences::DefaultCredentials input) {
  switch (input) {
    case net::HttpAuthPreferences::ALLOW_DEFAULT_CREDENTIALS:
      return network::mojom::DefaultCredentials::ALLOW_DEFAULT_CREDENTIALS;
    case net::HttpAuthPreferences::DISALLOW_DEFAULT_CREDENTIALS:
      return network::mojom::DefaultCredentials::DISALLOW_DEFAULT_CREDENTIALS;
  }
  NOTREACHED();
}

net::HttpAuthPreferences::DefaultCredentials
EnumTraits<network::mojom::DefaultCredentials,
           net::HttpAuthPreferences::DefaultCredentials>::
    FromMojom(network::mojom::DefaultCredentials input) {
  switch (input) {
    case network::mojom::DefaultCredentials::ALLOW_DEFAULT_CREDENTIALS:
      return net::HttpAuthPreferences::ALLOW_DEFAULT_CREDENTIALS;
    case network::mojom::DefaultCredentials::DISALLOW_DEFAULT_CREDENTIALS:
      return net::HttpAuthPreferences::DISALLOW_DEFAULT_CREDENTIALS;
  }
  NOTREACHED();
}

}  // namespace mojo
