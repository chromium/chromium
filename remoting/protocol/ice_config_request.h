// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ICE_CONFIG_REQUEST_H_
#define REMOTING_PROTOCOL_ICE_CONFIG_REQUEST_H_

#include "base/callback_forward.h"

namespace remoting {
namespace protocol {

struct IceConfig;

// Abstract interface used to fetch STUN and TURN configuration.
class IceConfigRequest {
 public:
  // Callback to receive results of the request. |ice_config| is null if the
  // request has failed.
  typedef base::OnceCallback<void(const IceConfig& ice_config)>
      OnIceConfigCallback;

  virtual ~IceConfigRequest() {}

  // Sends the request and calls the |callback| with the results.
  virtual void Send(OnIceConfigCallback callback) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_ICE_CONFIG_REQUEST_H_
