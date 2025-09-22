// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SSL_CONFIG_TYPE_CONVERTER_H_
#define SERVICES_NETWORK_SSL_CONFIG_TYPE_CONVERTER_H_

#include "net/cert/cert_verifier.h"
#include "net/ssl/ssl_config_service.h"
#include "services/network/public/mojom/ssl_config.mojom.h"

namespace mojo {

int MojoSSLVersionToNetSSLVersion(network::mojom::SSLVersion mojo_version);

// Converts a network::mojom::SSLConfigPtr to a net::SSLContextConfig. Tested in
// SSLConfigServiceMojo's unittests.
// Note: For net::SSLContextConfig::supported_named_groups, this does a little
// more than just converting types. It translates the simplified configuration
// surface exposed via network::mojom::{SSLConfig,SSLNamedGroupsPreset} into the
// underlying //net representation.
net::SSLContextConfig MojoSSLConfigToSSLContextConfig(
    const network::mojom::SSLConfigPtr& mojo_config);

// Converts a network::mojom::SSLConfigPtr to a net::CertVerifier::Config.
// Tested in SSLConfigServiceMojo's unittests.
net::CertVerifier::Config MojoSSLConfigToCertVerifierConfig(
    const network::mojom::SSLConfigPtr& mojo_config);

}  // namespace mojo

#endif  // SERVICES_NETWORK_SSL_CONFIG_TYPE_CONVERTER_H_
