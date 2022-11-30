// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_MOCK_MOJO_DHCP_WPAD_URL_CLIENT_H_
#define SERVICES_NETWORK_MOCK_MOJO_DHCP_WPAD_URL_CLIENT_H_

#include <memory>
#include <string>

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/dhcp_wpad_url_client.mojom.h"

namespace network {
class MockMojoDhcpWpadUrlClient : public network::mojom::DhcpWpadUrlClient {
 public:
  MockMojoDhcpWpadUrlClient(const std::string& pac_url);

  MockMojoDhcpWpadUrlClient(const MockMojoDhcpWpadUrlClient&) = delete;
  MockMojoDhcpWpadUrlClient& operator=(const MockMojoDhcpWpadUrlClient&) =
      delete;

  ~MockMojoDhcpWpadUrlClient() override;

  // Calls |callback| with |pac_url_|.
  void GetPacUrl(GetPacUrlCallback callback) override;

  // Convenience method that creates a self-owned
  // MockMojoDhcpWpadUrlClient and returns a remote endpoint to
  // control it.
  static mojo::PendingRemote<network::mojom::DhcpWpadUrlClient>
  CreateWithSelfOwnedReceiver(const std::string& pac_url);

 private:
  std::string pac_url_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_MOCK_MOJO_DHCP_WPAD_URL_CLIENT_H_
