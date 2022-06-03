// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_checksum.h"

namespace extensions {
namespace declarative_net_request {

RulesetChecksum::RulesetChecksum(RulesetID ruleset_id, int checksum)
    : ruleset_id(ruleset_id), checksum(checksum) {}

}  // namespace declarative_net_request
}  // namespace extensions
