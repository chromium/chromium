// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_MOCK_QUIC_CONTEXT_H_
#define NET_QUIC_MOCK_QUIC_CONTEXT_H_

#include "base/memory/raw_ptr.h"
#include "net/quic/quic_context.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"

namespace net {

class MockQuicContext : public QuicContext {
 public:
  MockQuicContext();
  ~MockQuicContext() override = default;

  void AdvanceTime(quic::QuicTime::Delta delta);

  quic::MockClock* mock_clock();

 private:
  raw_ptr<quic::test::MockQuicConnectionHelper> mock_helper_;
};

}  // namespace net

#endif  // NET_QUIC_MOCK_QUIC_CONTEXT_H_
