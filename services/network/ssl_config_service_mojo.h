// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SSL_CONFIG_SERVICE_MOJO_H_
#define SERVICES_NETWORK_SSL_CONFIG_SERVICE_MOJO_H_

#include <string_view>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cert/cert_verifier.h"
#include "net/ssl/ssl_config_service.h"
#include "services/network/public/mojom/ssl_config.mojom.h"

namespace network {

// An SSLConfigClient that serves as a net::SSLConfigService, listening to
// SSLConfig changes on a Mojo pipe, and providing access to the updated config.
class COMPONENT_EXPORT(NETWORK_SERVICE) SSLConfigServiceMojo
    : public mojom::SSLConfigClient,
      public net::SSLConfigService {
 public:
  // If |ssl_config_client_receiver| is not provided, just sticks with the
  // initial configuration.
  SSLConfigServiceMojo(
      mojom::SSLConfigPtr initial_config,
      mojo::PendingReceiver<mojom::SSLConfigClient> ssl_config_client_receiver);

  SSLConfigServiceMojo(const SSLConfigServiceMojo&) = delete;
  SSLConfigServiceMojo& operator=(const SSLConfigServiceMojo&) = delete;

  ~SSLConfigServiceMojo() override;

  // Sets |cert_verifier| to be configured by certificate-related settings
  // provided by the mojom::SSLConfigClient via OnSSLConfigUpdated. Once set,
  // |cert_verifier| must outlive the SSLConfigServiceMojo or be cleared by
  // passing nullptr as |cert_verifier| prior to destruction.
  void SetCertVerifierForConfiguring(net::CertVerifier* cert_verifier);

  // mojom::SSLConfigClient implementation:
  void OnSSLConfigUpdated(const mojom::SSLConfigPtr ssl_config) override;

  // net::SSLConfigService implementation:
  net::SSLContextConfig GetSSLContextConfig() override;
  bool CanShareConnectionWithClientCerts(
      std::string_view hostname) const override;

 private:
  mojo::Receiver<mojom::SSLConfigClient> receiver_{this};

  net::SSLContextConfig ssl_context_config_;
  net::CertVerifier::Config cert_verifier_config_;

  raw_ptr<net::CertVerifier> cert_verifier_;

  // The list of domains and subdomains from enterprise policy where connection
  // coalescing is allowed when client certs are in use if the hosts being
  // coalesced match this list.
  std::vector<std::string> client_cert_pooling_policy_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SSL_CONFIG_SERVICE_MOJO_H_
