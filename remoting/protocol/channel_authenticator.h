// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHANNEL_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_CHANNEL_AUTHENTICATOR_H_

#include <string>

#include "base/functional/callback_forward.h"

namespace remoting::protocol {

class P2PStreamSocket;

// Interface for channel authentications that perform channel-level
// authentication. Depending on implementation channel authenticators
// may also establish SSL connection. Each instance of this interface
// should be used only once for one channel.
class ChannelAuthenticator {
 public:
  typedef base::OnceCallback<void(int error, std::unique_ptr<P2PStreamSocket>)>
      DoneCallback;

  virtual ~ChannelAuthenticator() {}

  // Start authentication of the given |socket|. |done_callback| is called when
  // authentication is finished. Callback may be invoked before this method
  // returns, and may delete the calling authenticator.
  virtual void SecureAndAuthenticate(std::unique_ptr<P2PStreamSocket> socket,
                                     DoneCallback done_callback) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_CHANNEL_AUTHENTICATOR_H_
