// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ssl_config_type_converter.h"

#include "base/logging.h"

namespace mojo {

int MojoSSLVersionToNetSSLVersion(network::mojom::SSLVersion mojo_version) {
  switch (mojo_version) {
    case network::mojom::SSLVersion::kTLS1:
      return net::SSL_PROTOCOL_VERSION_TLS1;
    case network::mojom::SSLVersion::kTLS11:
      return net::SSL_PROTOCOL_VERSION_TLS1_1;
    case network::mojom::SSLVersion::kTLS12:
      return net::SSL_PROTOCOL_VERSION_TLS1_2;
    case network::mojom::SSLVersion::kTLS13:
      return net::SSL_PROTOCOL_VERSION_TLS1_3;
  }
  NOTREACHED();
  return net::SSL_PROTOCOL_VERSION_TLS1_3;
}

net::SSLContextConfig MojoSSLConfigToSSLContextConfig(
    const network::mojom::SSLConfigPtr& mojo_config) {
  net::SSLContextConfig net_config;

  net_config.version_min =
      MojoSSLVersionToNetSSLVersion(mojo_config->version_min);
  net_config.version_max =
      MojoSSLVersionToNetSSLVersion(mojo_config->version_max);
  DCHECK_LE(net_config.version_min, net_config.version_max);

  net_config.disabled_cipher_suites = mojo_config->disabled_cipher_suites;
  net_config.tls13_hardening_for_local_anchors_enabled =
      mojo_config->tls13_hardening_for_local_anchors_enabled;
  return net_config;
}

net::CertVerifier::Config MojoSSLConfigToCertVerifierConfig(
    const network::mojom::SSLConfigPtr& mojo_config) {
  net::CertVerifier::Config net_config;
  net_config.enable_rev_checking = mojo_config->rev_checking_enabled;
  net_config.require_rev_checking_local_anchors =
      mojo_config->rev_checking_required_local_anchors;
  net_config.enable_sha1_local_anchors =
      mojo_config->sha1_local_anchors_enabled;
  net_config.disable_symantec_enforcement =
      mojo_config->symantec_enforcement_disabled;

  return net_config;
}

}  // namespace mojo
