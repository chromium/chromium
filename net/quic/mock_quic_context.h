// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_MOCK_QUIC_CONTEXT_H_
#define NET_QUIC_MOCK_QUIC_CONTEXT_H_

#include "net/quic/quic_context.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"

namespace net {

class MockQuicContext : public QuicContext {
 public:
  MockQuicContext();

  void AdvanceTime(quic::QuicTime::Delta delta);

  quic::MockClock* mock_clock();

 private:
  quic::test::MockQuicConnectionHelper* mock_helper_;
};

}  // namespace net

#endif  // NET_QUIC_MOCK_QUIC_CONTEXT_H_
