// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_install_pref.h"

#include "base/check_op.h"

namespace extensions {
namespace declarative_net_request {

RulesetInstallPref::RulesetInstallPref(RulesetID ruleset_id,
                                       absl::optional<int> checksum,
                                       bool ignored)
    : ruleset_id(ruleset_id), checksum(checksum), ignored(ignored) {
  DCHECK_NE(ignored, checksum.has_value());
}

}  // namespace declarative_net_request
}  // namespace extensions
