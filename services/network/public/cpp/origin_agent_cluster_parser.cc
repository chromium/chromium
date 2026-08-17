// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/origin_agent_cluster_parser.h"
#include "net/http/structured_headers.h"

namespace network {

mojom::OriginAgentClusterValue ParseOriginAgentCluster(
    const std::string& header_value) {
  const auto item = net::structured_headers::ParseItem(header_value);
  const bool* boolean = item ? item->item.GetIfBoolean() : nullptr;
  if (!boolean) {
    return mojom::OriginAgentClusterValue::kAbsent;
  }
  return *boolean ? mojom::OriginAgentClusterValue::kTrue
                  : mojom::OriginAgentClusterValue::kFalse;
}

}  // namespace network
