// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ip_address_space_overrides_test_utils.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"

namespace network {
namespace {
// TODO(crbug.com/418737577): use network::IPAddressSpaceToStringPiece after
// address space rename finishes.
std::string_view IPAddressSpaceToStringPieceForOverride(
    mojom::IPAddressSpace space) {
  switch (space) {
    case mojom::IPAddressSpace::kUnknown:
      return "unknown";
    case mojom::IPAddressSpace::kPublic:
      return "public";
    case mojom::IPAddressSpace::kLocal:
      return "local";
    case mojom::IPAddressSpace::kLoopback:
      return "loopback";
  }
}

}  // namespace

void AddPublicIpAddressSpaceOverrideToCommandLine(
    const net::test_server::EmbeddedTestServer& server,
    base::CommandLine& command_line) {
  CHECK(server.Started());
  AddIpAddressSpaceOverridesToCommandLine(
      {GenerateIpAddressSpaceOverride(server, mojom::IPAddressSpace::kPublic)},
      command_line);
}

void AddIpAddressSpaceOverridesToCommandLine(
    base::span<const std::string> parts,
    base::CommandLine& command_line) {
  command_line.AppendSwitchASCII(network::switches::kIpAddressSpaceOverrides,
                                 base::JoinString(parts, ","));
}

std::string GenerateIpAddressSpaceOverride(
    const net::test_server::EmbeddedTestServer& server,
    mojom::IPAddressSpace space) {
  CHECK(server.Started());
  return base::StrCat({server.host_port_pair().ToString(), "=",
                       IPAddressSpaceToStringPieceForOverride(space)});
}

}  // namespace network
