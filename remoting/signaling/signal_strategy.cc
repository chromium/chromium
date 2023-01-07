// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/signal_strategy.h"

namespace remoting {

bool SignalStrategy::Listener::OnSignalStrategyIncomingMessage(
    const ftl::Id& sender_id,
    const std::string& sender_registration_id,
    const ftl::ChromotingMessage& message) {
  return false;
}

bool SignalStrategy::IsSignInError() const {
  return false;
}

}  // namespace remoting
