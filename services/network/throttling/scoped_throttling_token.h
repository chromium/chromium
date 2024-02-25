// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_THROTTLING_SCOPED_THROTTLING_TOKEN_H_
#define SERVICES_NETWORK_THROTTLING_SCOPED_THROTTLING_TOKEN_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/component_export.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace network {

// A scoped class that calls
// ThrottlingController::RegisterProfileIDForNetLogSource() in the constructor,
// and ThrottlingController::UnregisterNetLogSource() in the destructor.
class COMPONENT_EXPORT(NETWORK_SERVICE) ScopedThrottlingToken {
 public:
  // If |throttling_profile_id| is nullopt or there is no network throttling
  // conditions registered for the profile ID, returns nullptr. Otherwise
  // returns a new ScopedThrottlingToken. It must be kept until
  // ThrottlingNetworkTransaction::Start() will be called.
  static std::unique_ptr<ScopedThrottlingToken> MaybeCreate(
      uint32_t net_log_source_id,
      const std::optional<base::UnguessableToken>& throttling_profile_id);

  ScopedThrottlingToken(const ScopedThrottlingToken&) = delete;
  ScopedThrottlingToken& operator=(const ScopedThrottlingToken&) = delete;

  ~ScopedThrottlingToken();

 private:
  ScopedThrottlingToken(uint32_t net_log_source_id,
                        const base::UnguessableToken& throttling_profile_id);

  const uint32_t net_log_source_id_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_THROTTLING_SCOPED_THROTTLING_TOKEN_H_
