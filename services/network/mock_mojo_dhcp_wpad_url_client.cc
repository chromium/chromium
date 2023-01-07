// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/mock_mojo_dhcp_wpad_url_client.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace network {

MockMojoDhcpWpadUrlClient::MockMojoDhcpWpadUrlClient(const std::string& pac_url)
    : pac_url_(pac_url) {}

MockMojoDhcpWpadUrlClient::~MockMojoDhcpWpadUrlClient() = default;

void MockMojoDhcpWpadUrlClient::GetPacUrl(GetPacUrlCallback callback) {
  std::move(callback).Run(pac_url_);
}

mojo::PendingRemote<network::mojom::DhcpWpadUrlClient>
MockMojoDhcpWpadUrlClient::CreateWithSelfOwnedReceiver(
    const std::string& pac_url) {
  mojo::PendingRemote<network::mojom::DhcpWpadUrlClient> remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MockMojoDhcpWpadUrlClient>(pac_url),
      remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

}  // namespace network
