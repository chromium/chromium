// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ip_address_space_util.h"

#include <vector>

#include "base/command_line.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "net/base/ip_address.h"
#include "services/network/public/cpp/network_switches.h"

namespace network {
namespace {

using mojom::IPAddressSpace;

// Represents a single override of the form: subnet -> address space.
//
// We could name this `Override`, but `override` is a reserved keyword so that
// makes for awkward variable naming. Since the subnet is parsed from a CIDR
// block, `OverrideBlock` works well too.
class OverrideBlock {
 public:
  // Parses an override block from `str`, of the form "<CIDR block>=<space>".
  static base::Optional<OverrideBlock> Parse(base::StringPiece str);

  // Returns this block's address space if `address` belongs to this instance's
  // subnet. Returns nullopt otherwise.
  base::Optional<IPAddressSpace> Apply(const net::IPAddress& address) const;

 private:
  // Use `Parse()` instead.
  OverrideBlock() = default;

  net::IPAddress prefix_;
  size_t prefix_length_ = 0;
  IPAddressSpace space_ = IPAddressSpace::kUnknown;
};

base::Optional<IPAddressSpace> ParseIPAddressSpace(base::StringPiece str) {
  if (str == "public") {
    return IPAddressSpace::kPublic;
  }

  if (str == "private") {
    return IPAddressSpace::kPrivate;
  }

  if (str == "local") {
    return IPAddressSpace::kLocal;
  }

  return base::nullopt;
}

// static
base::Optional<OverrideBlock> OverrideBlock::Parse(base::StringPiece str) {
  std::vector<base::StringPiece> tokens = base::SplitStringPiece(
      str, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // There should be 2 parts: the CIDR block and the address space.
  if (tokens.size() != 2) {
    return base::nullopt;
  }

  base::StringPiece cidr = tokens[0];
  base::StringPiece address_space = tokens[1];

  OverrideBlock block;
  if (!net::ParseCIDRBlock(cidr, &block.prefix_, &block.prefix_length_)) {
    return base::nullopt;
  }

  base::Optional<IPAddressSpace> parsed_address_space =
      ParseIPAddressSpace(address_space);
  if (!parsed_address_space.has_value()) {
    return base::nullopt;
  }

  block.space_ = *parsed_address_space;
  return block;
}

base::Optional<IPAddressSpace> OverrideBlock::Apply(
    const net::IPAddress& address) const {
  if (net::IPAddressMatchesPrefix(address, prefix_, prefix_length_)) {
    return space_;
  }

  return base::nullopt;
}

// Parses a comma-separated list of override blocks. Ignores invalid blocks.
std::vector<OverrideBlock> ParseOverrideBlockList(base::StringPiece list) {
  // Since we skip invalid entries anyway, we can skip empty entries.
  std::vector<base::StringPiece> tokens = base::SplitStringPiece(
      list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::vector<OverrideBlock> blocks;
  for (base::StringPiece token : tokens) {
    base::Optional<OverrideBlock> block = OverrideBlock::Parse(token);
    if (block.has_value()) {
      blocks.push_back(*std::move(block));
    }
  }

  return blocks;
}

// Applies override blocks specified on the command-line to `address`.
// Returns nullopt if no override block matches `address`.
base::Optional<IPAddressSpace> ApplyCommandLineOverrides(
    const net::IPAddress& address) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kIpAddressSpaceOverrides)) {
    return base::nullopt;
  }

  std::vector<OverrideBlock> blocks = ParseOverrideBlockList(
      command_line.GetSwitchValueASCII(switches::kIpAddressSpaceOverrides));
  for (const OverrideBlock& block : blocks) {
    base::Optional<IPAddressSpace> space = block.Apply(address);
    if (space.has_value()) {
      return space;
    }
  }

  return base::nullopt;
}

}  // namespace

IPAddressSpace IPAddressToIPAddressSpace(const net::IPAddress& address) {
  if (!address.IsValid()) {
    return IPAddressSpace::kUnknown;
  }

  base::Optional<IPAddressSpace> space = ApplyCommandLineOverrides(address);
  if (space.has_value()) {
    return *space;
  }

  if (address.IsLoopback()) {
    return IPAddressSpace::kLocal;
  }

  if (!address.IsPubliclyRoutable()) {
    return IPAddressSpace::kPrivate;
  }

  return IPAddressSpace::kPublic;
}

namespace {

// For comparison purposes, we treat kUnknown the same as kPublic.
IPAddressSpace CollapseUnknown(IPAddressSpace space) {
  if (space == IPAddressSpace::kUnknown) {
    return IPAddressSpace::kPublic;
  }
  return space;
}

}  // namespace

bool IsLessPublicAddressSpace(IPAddressSpace lhs, IPAddressSpace rhs) {
  // Apart from the special case for kUnknown, the built-in comparison operator
  // works just fine. The comment on IPAddressSpace's definition notes that the
  // enum values' ordering matters.
  return CollapseUnknown(lhs) < CollapseUnknown(rhs);
}

}  // namespace network
