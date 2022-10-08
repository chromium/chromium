// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/alternate_protocol_usage_mojom_traits.h"

namespace mojo {

network::mojom::AlternateProtocolUsage EnumTraits<
    network::mojom::AlternateProtocolUsage,
    net::AlternateProtocolUsage>::ToMojom(net::AlternateProtocolUsage input) {
  switch (input) {
    case net::ALTERNATE_PROTOCOL_USAGE_NO_RACE:
      return network::mojom::AlternateProtocolUsage::kNoRace;
    case net::ALTERNATE_PROTOCOL_USAGE_WON_RACE:
      return network::mojom::AlternateProtocolUsage::kWonRace;
    case net::ALTERNATE_PROTOCOL_USAGE_MAIN_JOB_WON_RACE:
      return network::mojom::AlternateProtocolUsage::kMainJobWonRace;
    case net::ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING:
      return network::mojom::AlternateProtocolUsage::kMappingMissing;
    case net::ALTERNATE_PROTOCOL_USAGE_BROKEN:
      return network::mojom::AlternateProtocolUsage::kBroken;
    case net::ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_WITHOUT_RACE:
      return network::mojom::AlternateProtocolUsage::
          kDnsAlpnH3JobWonWithoutRace;
    case net::ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_RACE:
      return network::mojom::AlternateProtocolUsage::kDnsAlpnH3JobWonRace;
    default:
      return network::mojom::AlternateProtocolUsage::kUnspecifiedReason;
  }
}

bool EnumTraits<network::mojom::AlternateProtocolUsage,
                net::AlternateProtocolUsage>::
    FromMojom(network::mojom::AlternateProtocolUsage input,
              net::AlternateProtocolUsage* output) {
  switch (input) {
    case network::mojom::AlternateProtocolUsage::kNoRace:
      *output = net::ALTERNATE_PROTOCOL_USAGE_NO_RACE;
      return true;
    case network::mojom::AlternateProtocolUsage::kWonRace:
      *output = net::ALTERNATE_PROTOCOL_USAGE_WON_RACE;
      return true;
    case network::mojom::AlternateProtocolUsage::kMainJobWonRace:
      *output = net::ALTERNATE_PROTOCOL_USAGE_MAIN_JOB_WON_RACE;
      return true;
    case network::mojom::AlternateProtocolUsage::kMappingMissing:
      *output = net::ALTERNATE_PROTOCOL_USAGE_MAPPING_MISSING;
      return true;
    case network::mojom::AlternateProtocolUsage::kBroken:
      *output = net::ALTERNATE_PROTOCOL_USAGE_BROKEN;
      return true;
    case network::mojom::AlternateProtocolUsage::kDnsAlpnH3JobWonWithoutRace:
      *output = net::ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_WITHOUT_RACE;
      return true;
    case network::mojom::AlternateProtocolUsage::kDnsAlpnH3JobWonRace:
      *output = net::ALTERNATE_PROTOCOL_USAGE_DNS_ALPN_H3_JOB_WON_RACE;
      return true;
    case network::mojom::AlternateProtocolUsage::kMaxValue:
      *output = net::ALTERNATE_PROTOCOL_USAGE_MAX;
      return true;
    default:
      return false;
  }
}

}  // namespace mojo