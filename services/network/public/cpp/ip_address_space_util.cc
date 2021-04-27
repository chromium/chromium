// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ip_address_space_util.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "net/base/ip_address.h"
#include "services/network/public/cpp/network_switches.h"

namespace network {
namespace {

using mojom::IPAddressSpace;
using net::IPAddress;

// Represents a single override of the form: subnet -> address space.
//
// We could name this `Override`, but `override` is a reserved keyword so that
// makes for awkward variable naming. Since the subnet is parsed from a CIDR
// block, `OverrideBlock` works well too.
class OverrideBlock {
 public:
  OverrideBlock(IPAddress prefix, size_t prefix_length, IPAddressSpace space)
      : prefix_(std::move(prefix)),
        prefix_length_(prefix_length),
        space_(space) {}

  // Parses an override block from `str`, of the form "<CIDR block>=<space>".
  static base::Optional<OverrideBlock> Parse(base::StringPiece str);

  // Returns this block's address space if `address` belongs to this instance's
  // subnet. Returns nullopt otherwise.
  base::Optional<IPAddressSpace> Apply(const IPAddress& address) const;

 private:
  // Use `Parse()` instead.
  OverrideBlock() = default;

  IPAddress prefix_;
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
    const IPAddress& address) const {
  if (net::IPAddressMatchesPrefix(address, prefix_, prefix_length_)) {
    return space_;
  }

  return base::nullopt;
}

// Represents a sequential list of `OverrideBlock` instances.
class OverrideBlockList {
 public:
  explicit OverrideBlockList(std::vector<OverrideBlock> blocks)
      : blocks_(std::move(blocks)) {}

  // Parses a comma-separated list of override blocks. Ignores invalid blocks.
  static OverrideBlockList Parse(base::StringPiece str);

  // Applies blocks in this list to `address`, in sequential order. Returns the
  // address space of the first matching override block. Returns nullopt if no
  // match is found.
  base::Optional<IPAddressSpace> Apply(const IPAddress& address) const;

 private:
  std::vector<OverrideBlock> blocks_;
};

// static
OverrideBlockList OverrideBlockList::Parse(base::StringPiece list) {
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

  return OverrideBlockList(std::move(blocks));
}

base::Optional<IPAddressSpace> OverrideBlockList::Apply(
    const IPAddress& address) const {
  base::Optional<IPAddressSpace> space;

  for (const OverrideBlock& block : blocks_) {
    space = block.Apply(address);
    if (space.has_value()) {
      break;
    }
  }

  // If we never found a match, `space` is still `nullopt`.
  return space;
}

// Applies override blocks specified on the command-line to `address`.
// Returns nullopt if no override block matches `address`.
base::Optional<IPAddressSpace> ApplyCommandLineOverrides(
    const IPAddress& address) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kIpAddressSpaceOverrides)) {
    return base::nullopt;
  }

  std::string switch_str =
      command_line.GetSwitchValueASCII(switches::kIpAddressSpaceOverrides);
  return OverrideBlockList::Parse(switch_str).Apply(address);
}

// Returns a block list containing all default-non-public subnets.
const OverrideBlockList& NonPublicBlockList() {
  // Have to repeat `OverrideBlockList` because perfect forwarding does not deal
  // well with initializer lists.
  static const base::NoDestructor<OverrideBlockList> kBlocks(OverrideBlockList({
      // IPv6 Loopback (RFC 4291): ::1/128
      OverrideBlock(IPAddress::IPv6Localhost(), 128, IPAddressSpace::kLocal),
      // IPv6 Unique-local (RFC 4193, RFC 8190): fc00::/7
      OverrideBlock(
          IPAddress(0xfc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 7,
          IPAddressSpace::kPrivate),
      // IPv6 Link-local unicast (RFC 4291): fe80::/10
      OverrideBlock(
          IPAddress(0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 10,
          IPAddressSpace::kPrivate),
      // IPv4 Loopback (RFC 1122): 127.0.0.0/8
      OverrideBlock(IPAddress(127, 0, 0, 0), 8, IPAddressSpace::kLocal),
      // IPv4 Private use (RFC 1918): 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16
      OverrideBlock(IPAddress(10, 0, 0, 0), 8, IPAddressSpace::kPrivate),
      OverrideBlock(IPAddress(172, 16, 0, 0), 12, IPAddressSpace::kPrivate),
      OverrideBlock(IPAddress(192, 168, 0, 0), 16, IPAddressSpace::kPrivate),
      // IPv4 Link-local (RFC 3927): 169.254.0.0/16
      OverrideBlock(IPAddress(169, 254, 0, 0), 16, IPAddressSpace::kPrivate),
  }));
  return *kBlocks;
}

}  // namespace

IPAddressSpace IPAddressToIPAddressSpace(const IPAddress& address) {
  if (!address.IsValid()) {
    return IPAddressSpace::kUnknown;
  }

  base::Optional<IPAddressSpace> space = ApplyCommandLineOverrides(address);
  if (space.has_value()) {
    return *space;
  }

  return NonPublicBlockList().Apply(address).value_or(IPAddressSpace::kPublic);
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
