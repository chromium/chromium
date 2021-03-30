// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/origin_agent_cluster_parser.h"
#include "net/http/structured_headers.h"

namespace network {

bool ParseOriginAgentCluster(const std::string& header_value) {
  const auto item = net::structured_headers::ParseItem(header_value);
  return item && item->item.is_boolean() && item->item.GetBoolean();
}

}  // namespace network
