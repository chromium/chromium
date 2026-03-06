// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FTL_ECHO_MESSAGE_LISTENER_H_
#define REMOTING_HOST_FTL_ECHO_MESSAGE_LISTENER_H_

#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "remoting/signaling/ftl_signal_strategy.h"

namespace remoting {

// FtlEchoMessageListener listens for, and responds to, echo messages which have
// been sent to this endpoint via the signaling channel.  The most common usage
// is determine whether this endpoint is reachable without requiring the
// construction of a well-formed XMPP stanza and won't interfere with the
// standard signaling process if sent mid-connection negotiation.
class FtlEchoMessageListener : public FtlSignalStrategy::FtlListener {
 public:
  using CheckAccessPermissionCallback =
      base::RepeatingCallback<bool(std::string_view)>;

  // |signal_strategy| is expected to outlive this object.
  FtlEchoMessageListener(CheckAccessPermissionCallback callback,
                         FtlSignalStrategy* ftl_signal_strategy);

  FtlEchoMessageListener(const FtlEchoMessageListener&) = delete;
  FtlEchoMessageListener& operator=(const FtlEchoMessageListener&) = delete;

  ~FtlEchoMessageListener() override;

  // FtlSignalStrategy::FtlListener interface.
  bool OnIncomingFtlMessage(const SignalingAddress& sender_address,
                            const ftl::ChromotingMessage& message) override;

 private:
  CheckAccessPermissionCallback check_access_permission_callback_;
  raw_ptr<FtlSignalStrategy> ftl_signal_strategy_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FTL_ECHO_MESSAGE_LISTENER_H_
