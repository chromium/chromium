// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/mock_quic_context.h"

namespace net {

MockQuicContext::MockQuicContext()
    : QuicContext(std::make_unique<quic::test::MockQuicConnectionHelper>()) {
  mock_helper_ = static_cast<quic::test::MockQuicConnectionHelper*>(helper());
}

void MockQuicContext::AdvanceTime(quic::QuicTime::Delta delta) {
  mock_helper_->AdvanceTime(delta);
}

quic::MockClock* MockQuicContext::mock_clock() {
  // TODO(vasilvv): add a proper accessor to MockQuicConnectionHelper and delete
  // the cast.
  return const_cast<quic::MockClock*>(
      static_cast<const quic::MockClock*>(mock_helper_->GetClock()));
}

}  // namespace net
