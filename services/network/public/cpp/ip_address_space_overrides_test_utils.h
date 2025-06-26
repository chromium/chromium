// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_OVERRIDES_TEST_UTILS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_OVERRIDES_TEST_UTILS_H_

#include <string>

#include "base/containers/span.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"

namespace base {
class CommandLine;
}

namespace net::test_server {
class EmbeddedTestServer;
}

namespace network {

// These functions are all helper functions to make addition of IP Address Space
// overrides for net::test_server::EmbeddedTestServers easier.
//
// IP Address Space overrides are needed for EmbeddedTestServers that are the
// targets of web requests that are considered Local Network Access (LNA). LNA
// requests are those where the requesting page is more public than the resource
// that is being requested. Since most EmbeddedTestServers are run on 127.0.0.1,
// this can catch many browser tests. For more background, see the comment for
// kIpAddressSpaceOverrides in services/network/public/cpp/network_switches.cc.
//
// Most calls will look something this:
//
// network::AddPublicIpAddressSpaceOverrideToCommandLine(
//    embedded_test_server, command_line);
//
// which will mark all requests to embedded_test_server in the kPublic address
// space.
//
// If you have multiple servers, this might look like the following:
//
//  network::AddIpAddressSpaceOverridesToCommandLine(
//      {network::GenerateIpAddressSpaceOverride(embedded_server_1),
//       network::GenerateIpAddressSpaceOverride(
//           embedded_server_2, network::mojom::IPAddressSpace::kLoopback)},
//      command_line);
//
// All net::test_server::EmbeddedTestServers passed in must have their ports
// assigned. This can be done by calling either
// EmbeddedTestServer::InitializeAndListen() or EmbeddedTestServer::Start()
// before calling the below functions.

// Ensures resources hosted by the server are treated as in the public IP
// Address Space by LNA, using the network::switches::kIpAddressSpaceOverrides
// command line switch.
//
// This appends to command_line, and will overwrite any previous overrides added
// to the command_line.
void AddPublicIpAddressSpaceOverrideToCommandLine(
    const net::test_server::EmbeddedTestServer& server,
    base::CommandLine& command_line);

// Appends IP Address Space overrides to the command line using the
// the network::switches::kIpAddressSpaceOverrides command line switch. Will
// overwrite any other IP Address Space overrides already applied.
//
// This is commonly used in conjunction with GenerateIpAddressSpaceOverride().
void AddIpAddressSpaceOverridesToCommandLine(
    base::span<const std::string> parts,
    base::CommandLine& command_line);

// Generates an IP Address space override part indicating that resources hosted
// by the server are in the specified IP Address Space.
//
// This is commonly used in conjunction with
// AddIpAddressSpaceOverridesToCommandLine().
std::string GenerateIpAddressSpaceOverride(
    const net::test_server::EmbeddedTestServer& server,
    mojom::IPAddressSpace space = mojom::IPAddressSpace::kPublic);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_IP_ADDRESS_SPACE_OVERRIDES_TEST_UTILS_H_
