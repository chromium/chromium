// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/mock_signal_strategy.h"

namespace remoting {

MockSignalStrategy::MockSignalStrategy(const SignalingAddress& address)
    : local_address_(address) {}
MockSignalStrategy::~MockSignalStrategy() = default;

const SignalingAddress& MockSignalStrategy::GetLocalAddress() const {
  return local_address_;
}

}  // namespace remoting
