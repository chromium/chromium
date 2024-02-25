// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_checksum.h"

namespace extensions::declarative_net_request {

RulesetChecksum::RulesetChecksum(RulesetID ruleset_id, int checksum)
    : ruleset_id(ruleset_id), checksum(checksum) {}

}  // namespace extensions::declarative_net_request
