// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/signal_strategy.h"

#include "remoting/signaling/signaling_address.h"

namespace remoting {

bool SignalStrategy::Listener::OnSignalStrategyIncomingMessage(
    const SignalingAddress& sender_address,
    const SignalingMessage& message) {
  return false;
}

bool SignalStrategy::IsSignInError() const {
  return false;
}

}  // namespace remoting
