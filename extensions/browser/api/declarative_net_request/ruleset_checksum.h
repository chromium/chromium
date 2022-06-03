// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_CHECKSUM_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_CHECKSUM_H_

#include <vector>

#include "extensions/common/api/declarative_net_request/constants.h"

namespace extensions {
namespace declarative_net_request {

struct RulesetChecksum {
  RulesetChecksum(RulesetID ruleset_id, int checksum);

  // ID of the ruleset.
  RulesetID ruleset_id;
  // Checksum of the indexed ruleset.
  int checksum;
};

using RulesetChecksums = std::vector<RulesetChecksum>;

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_CHECKSUM_H_
