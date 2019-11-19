// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_context.h"

#include "net/quic/platform/impl/quic_chromium_clock.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_random.h"

namespace net {

QuicContext::QuicContext()
    : QuicContext(std::make_unique<QuicChromiumConnectionHelper>(
          quic::QuicChromiumClock::GetInstance(),
          quic::QuicRandom::GetInstance())) {}

QuicContext::QuicContext(
    std::unique_ptr<quic::QuicConnectionHelperInterface> helper)
    : helper_(std::move(helper)) {}

QuicContext::~QuicContext() = default;

}  // namespace net
