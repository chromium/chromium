// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_key.h"

#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/socket/socket_tag.h"
#include "url/scheme_host_port.h"

namespace net {

HttpStreamKey::HttpStreamKey() = default;

HttpStreamKey::HttpStreamKey(url::SchemeHostPort destination,
                             PrivacyMode privacy_mode,
                             SocketTag socket_tag,
                             NetworkAnonymizationKey network_anonymization_key,
                             SecureDnsPolicy secure_dns_policy,
                             bool disable_cert_network_fetches)
    : destination_(std::move(destination)),
      privacy_mode_(privacy_mode),
      socket_tag_(std::move(socket_tag)),
      network_anonymization_key_(std::move(network_anonymization_key)),
      secure_dns_policy_(secure_dns_policy),
      disable_cert_network_fetches_(disable_cert_network_fetches) {
  CHECK(socket_tag_ == SocketTag()) << "Socket tag is not supported yet";
}

HttpStreamKey::~HttpStreamKey() = default;

HttpStreamKey::HttpStreamKey(const HttpStreamKey& other) = default;

HttpStreamKey& HttpStreamKey::operator=(const HttpStreamKey& other) = default;

bool HttpStreamKey::operator==(const HttpStreamKey& other) const = default;

bool HttpStreamKey::operator<(const HttpStreamKey& other) const {
  return std::tie(destination_, privacy_mode_, socket_tag_,
                  network_anonymization_key_, secure_dns_policy_,
                  disable_cert_network_fetches_) <
         std::tie(other.destination_, other.privacy_mode_, other.socket_tag_,
                  other.network_anonymization_key_, other.secure_dns_policy_,
                  other.disable_cert_network_fetches_);
}

}  // namespace net
