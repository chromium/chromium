// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ssl_config_service_mojo.h"

#include <string_view>

#include "base/strings/string_util.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "services/network/ssl_config_type_converter.h"

namespace network {

namespace {

// Returns true if |hostname| is a subdomain of |pattern| (including if they are
// equal).
bool IsSubdomain(std::string_view hostname, std::string_view pattern) {
  if (hostname == pattern) {
    return true;
  }
  if (hostname.length() <= (pattern.length() + 1)) {
    return false;
  }
  if (!base::EndsWith(hostname, pattern)) {
    return false;
  }
  return hostname[hostname.length() - pattern.length() - 1] == '.';
}

}  // namespace

SSLConfigServiceMojo::SSLConfigServiceMojo(
    mojom::SSLConfigPtr initial_config,
    mojo::PendingReceiver<mojom::SSLConfigClient> ssl_config_client_receiver,
    std::unique_ptr<net::EchModeGetter> ech_mode_getter)
    : ech_mode_getter_(std::move(ech_mode_getter)),
      client_cert_pooling_policy_(
          initial_config ? initial_config->client_cert_pooling_policy
                         : std::vector<std::string>()) {
  if (initial_config) {
    ech_enabled_ = initial_config->ech_enabled;
    cert_verifier_config_ = MojoSSLConfigToCertVerifierConfig(initial_config);
    ssl_context_config_ = MojoSSLConfigToSSLContextConfig(initial_config);
  }

  if (ssl_config_client_receiver)
    receiver_.Bind(std::move(ssl_config_client_receiver));
}

SSLConfigServiceMojo::~SSLConfigServiceMojo() = default;

void SSLConfigServiceMojo::SetCertVerifierForConfiguring(
    net::CertVerifier* cert_verifier) {
  cert_verifier_ = cert_verifier;
  if (cert_verifier_) {
    cert_verifier_->SetConfig(cert_verifier_config_);
  }
}

void SSLConfigServiceMojo::OnSSLConfigUpdated(mojom::SSLConfigPtr ssl_config) {
  // Connection pooling logic does not take into account EchMode. If ECH is
  // enabled, or disabled, by a change in policy, force an update to clean up
  // any existing connection. This makes it sure that we abide the new ECH
  // policy. Note: we don't account for EchMode in the pooling logic because a
  // change in policy is the only way to change EchMode for a given host.
  bool force_notification =
      client_cert_pooling_policy_ != ssl_config->client_cert_pooling_policy ||
      ech_enabled_ != ssl_config->ech_enabled;
  ech_enabled_ = ssl_config->ech_enabled;
  client_cert_pooling_policy_ = ssl_config->client_cert_pooling_policy;

  net::SSLContextConfig old_config = ssl_context_config_;
  ssl_context_config_ = MojoSSLConfigToSSLContextConfig(ssl_config);
  ProcessConfigUpdate(old_config, ssl_context_config_, force_notification);

  net::CertVerifier::Config old_cert_verifier_config = cert_verifier_config_;
  cert_verifier_config_ = MojoSSLConfigToCertVerifierConfig(ssl_config);
  if (cert_verifier_ && (old_cert_verifier_config != cert_verifier_config_)) {
    cert_verifier_->SetConfig(cert_verifier_config_);
  }
}

net::SSLContextConfig SSLConfigServiceMojo::GetSSLContextConfig() {
  return ssl_context_config_;
}

net::EchMode SSLConfigServiceMojo::GetEchMode(std::string_view hostname) const {
  if (!ech_enabled_) {
    return net::EchMode::kDisabled;
  }
  if (ech_mode_getter_) {
    return ech_mode_getter_->GetEchMode(hostname);
  }
  return net::EchMode::kOpportunistic;
}

bool SSLConfigServiceMojo::CanShareConnectionWithClientCerts(
    std::string_view hostname) const {
  // Hostnames (and the patterns configured for this class) must be
  // canonicalized before comparison, or the comparison will fail.
  for (const std::string& pattern : client_cert_pooling_policy_) {
    if (pattern.empty()) {
      continue;
    }
    // If the pattern starts with a '.', |hostname| must match it exactly
    // (except for the leading dot) for the pattern to be a match.
    if (pattern[0] == '.') {
      if (pattern.compare(1, std::string::npos, hostname) == 0) {
        return true;
      } else {
        continue;
      }
    }
    // Patterns that don't start with a dot match subdomains as well as an exact
    // pattern match. For example, a pattern of "example.com" should match a
    // hostname of "example.com", "sub.example.com", but not "notexample.com".
    if (IsSubdomain(hostname, pattern)) {
      return true;
    }
  }
  return false;
}

}  // namespace network
