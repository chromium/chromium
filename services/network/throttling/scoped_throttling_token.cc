// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/throttling/scoped_throttling_token.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/unguessable_token.h"
#include "services/network/throttling/throttling_controller.h"

namespace network {

// static
std::unique_ptr<ScopedThrottlingToken> ScopedThrottlingToken::MaybeCreate(
    uint32_t net_log_source_id,
    const std::optional<base::UnguessableToken>& throttling_profile_id) {
  if (!throttling_profile_id)
    return nullptr;

  return base::WrapUnique(
      new ScopedThrottlingToken(net_log_source_id, *throttling_profile_id));
}

ScopedThrottlingToken::ScopedThrottlingToken(
    uint32_t net_log_source_id,
    const base::UnguessableToken& throttling_profile_id)
    : net_log_source_id_(net_log_source_id) {
  ThrottlingController::RegisterProfileIDForNetLogSource(net_log_source_id,
                                                         throttling_profile_id);
}

ScopedThrottlingToken::~ScopedThrottlingToken() {
  ThrottlingController::UnregisterNetLogSource(net_log_source_id_);
}

}  // namespace network
