// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ICE_CONFIG_FETCHER_H_
#define REMOTING_PROTOCOL_ICE_CONFIG_FETCHER_H_

#include <optional>

#include "base/functional/callback_forward.h"

namespace remoting::protocol {

struct IceConfig;

// Abstract interface used to fetch STUN and TURN configuration information.
class IceConfigFetcher {
 public:
  // |ice_config| is the result of the service request or empty on error.
  typedef base::OnceCallback<void(std::optional<IceConfig> ice_config)>
      OnIceConfigCallback;

  virtual ~IceConfigFetcher() = default;

  // Makes a service request and calls the |callback| with the result.
  virtual void GetIceConfig(OnIceConfigCallback callback) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_ICE_CONFIG_FETCHER_H_
