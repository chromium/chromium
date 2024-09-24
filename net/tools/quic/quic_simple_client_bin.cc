// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A binary wrapper for QuicClient.
// Connects to a host using QUIC, sends a request to the provided URL, and
// displays the response.
//
// Some usage examples:
//
// Standard request/response:
//   quic_client http://www.google.com
//   quic_client http://www.google.com --quiet
//   quic_client https://www.google.com --port=443
//
// Use a specific version:
//   quic_client http://www.google.com --quic_version=23
//
// Send a POST instead of a GET:
//   quic_client http://www.google.com --body="this is a POST body"
//
// Append additional headers to the request:
//   quic_client http://www.google.com  --host=${IP}
//               --headers="Header-A: 1234; Header-B: 5678"
//
// Connect to a host different to the URL being requested:
//   quic_client mail.google.com --host=www.google.com
//
// Connect to a specific IP:
//   IP=`dig www.google.com +short | head -1`
//   quic_client www.google.com --host=${IP}
//
// Try to connect to a host which does not speak QUIC:
//   quic_client http://www.example.com

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "net/base/address_family.h"
#include "net/base/net_errors.h"
#include "net/quic/address_utils.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_command_line_flags.h"
#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_system_event_loop.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quiche/src/quiche/quic/tools/quic_toy_client.h"
#include "net/tools/quic/quic_simple_client.h"
#include "net/tools/quic/synchronous_host_resolver.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

using quic::ProofVerifier;

namespace {

class QuicSimpleClientFactory : public quic::QuicToyClient::ClientFactory {
 public:
  std::unique_ptr<quic::QuicSpdyClientBase> CreateClient(
      std::string host_for_handshake,
      std::string host_for_lookup,
      int address_family_for_lookup,
      uint16_t port,
      quic::ParsedQuicVersionVector versions,
      const quic::QuicConfig& config,
      std::unique_ptr<quic::ProofVerifier> verifier,
      std::unique_ptr<quic::SessionCache> /*session_cache*/) override {
    // Determine IP address to connect to from supplied hostname.
    quic::QuicIpAddress ip_addr;
    if (!ip_addr.FromString(host_for_lookup)) {
      net::AddressList addresses;
      // TODO(crbug.com/40216365) Let the caller pass in the scheme
      // rather than guessing "https"
      int rv = net::SynchronousHostResolver::Resolve(
          url::SchemeHostPort(url::kHttpsScheme, host_for_lookup, port),
          &addresses);
      if (rv != net::OK) {
        LOG(ERROR) << "Unable to resolve '" << host_for_lookup
                   << "' : " << net::ErrorToShortString(rv);
        return nullptr;
      }
      const auto endpoint = base::ranges::find_if(
          addresses,
          [address_family_for_lookup](net::AddressFamily family) {
            if (address_family_for_lookup == AF_INET)
              return family == net::AddressFamily::ADDRESS_FAMILY_IPV4;
            if (address_family_for_lookup == AF_INET6)
              return family == net::AddressFamily::ADDRESS_FAMILY_IPV6;
            return address_family_for_lookup == AF_UNSPEC;
          },
          &net::IPEndPoint::GetFamily);
      if (endpoint == addresses.end()) {
        LOG(ERROR) << "No results for '" << host_for_lookup
                   << "' with appropriate address family";
        return nullptr;
      }
      // Arbitrarily select the first result with a matching address family,
      // ignoring any subsequent matches.
      ip_addr = net::ToQuicIpAddress(endpoint->address());
      port = endpoint->port();
    }

    quic::QuicServerId server_id(host_for_handshake, port);
    return std::make_unique<net::QuicSimpleClient>(
        quic::QuicSocketAddress(ip_addr, port), server_id, versions, config,
        std::move(verifier));
  }
};

}  // namespace

int main(int argc, char* argv[]) {
  quiche::QuicheSystemEventLoop event_loop("quic_client");
  const char* usage = "Usage: quic_client [options] <url>";

  // All non-flag arguments should be interpreted as URLs to fetch.
  std::vector<std::string> urls =
      quiche::QuicheParseCommandLineFlags(usage, argc, argv);
  if (urls.size() != 1) {
    quiche::QuichePrintCommandLineFlagHelp(usage);
    exit(0);
  }

  QuicSimpleClientFactory factory;
  quic::QuicToyClient client(&factory);
  return client.SendRequestsAndPrintResponses(urls);
}
