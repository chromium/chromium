// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/default_credentials_mojom_traits.h"

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
  NOTREACHED_IN_MIGRATION();
  return network::mojom::DefaultCredentials::ALLOW_DEFAULT_CREDENTIALS;
}

bool EnumTraits<network::mojom::DefaultCredentials,
                net::HttpAuthPreferences::DefaultCredentials>::
    FromMojom(network::mojom::DefaultCredentials input,
              net::HttpAuthPreferences::DefaultCredentials* output) {
  switch (input) {
    case network::mojom::DefaultCredentials::ALLOW_DEFAULT_CREDENTIALS:
      *output = net::HttpAuthPreferences::ALLOW_DEFAULT_CREDENTIALS;
      return true;
    case network::mojom::DefaultCredentials::DISALLOW_DEFAULT_CREDENTIALS:
      *output = net::HttpAuthPreferences::DISALLOW_DEFAULT_CREDENTIALS;
      return true;
  }
  return false;
}

}  // namespace mojo
