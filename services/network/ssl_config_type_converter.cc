// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ssl_config_type_converter.h"

#include <functional>
#include <optional>

#include "base/check_op.h"
#include "base/notreached.h"
#include "net/ssl/ssl_config_service.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace mojo {

int MojoSSLVersionToNetSSLVersion(network::mojom::SSLVersion mojo_version) {
  switch (mojo_version) {
    case network::mojom::SSLVersion::kTLS12:
      return net::SSL_PROTOCOL_VERSION_TLS1_2;
    case network::mojom::SSLVersion::kTLS13:
      return net::SSL_PROTOCOL_VERSION_TLS1_3;
  }
  NOTREACHED();
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
  net_config.tls13_cipher_prefer_aes_256 =
      mojo_config->tls13_cipher_prefer_aes_256;
  net_config.ech_enabled = mojo_config->ech_enabled;

  // Translate the configuration options related to named groups.
  switch (mojo_config->named_groups_preset) {
    case network::mojom::SSLNamedGroupsPreset::kDefault:
      // Do nothing, the `net::SSLContextConfig` constructor starts with the
      // default list.
      break;
    case network::mojom::SSLNamedGroupsPreset::kCnsa2:
      net_config.supported_named_groups = {
          {.group_id = SSL_GROUP_MLKEM1024, .send_key_share = false},
          {.group_id = SSL_GROUP_X25519_MLKEM768, .send_key_share = true},
          {.group_id = SSL_GROUP_SECP384R1, .send_key_share = false},
          {.group_id = SSL_GROUP_SECP256R1, .send_key_share = false},
          {.group_id = SSL_GROUP_X25519, .send_key_share = true},
      };
      break;
  }
  if (!mojo_config->post_quantum_key_agreement_enabled) {
    std::erase_if(net_config.supported_named_groups,
                  std::mem_fn(&net::SSLNamedGroupInfo::IsPostQuantum));
  }

  for (const auto& tai : mojo_config->trust_anchor_ids) {
    net_config.trust_anchor_ids.insert(tai);
  }

  for (const auto& tai : mojo_config->mtc_trust_anchor_ids) {
    net_config.mtc_trust_anchor_ids.push_back(tai);
  }

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

  return net_config;
}

}  // namespace mojo
