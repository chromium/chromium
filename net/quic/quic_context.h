// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CONTEXT_H_
#define NET_QUIC_QUIC_CONTEXT_H_

#include <memory>

#include "net/third_party/quiche/src/quic/core/quic_connection.h"

namespace net {

// QuicContext contains QUIC-related variables that are shared across all of the
// QUIC connections, both HTTP and non-HTTP ones.
class NET_EXPORT_PRIVATE QuicContext {
 public:
  QuicContext();
  QuicContext(std::unique_ptr<quic::QuicConnectionHelperInterface> helper);
  ~QuicContext();

  quic::QuicConnectionHelperInterface* helper() { return helper_.get(); }
  const quic::QuicClock* clock() { return helper_->GetClock(); }
  quic::QuicRandom* random_generator() { return helper_->GetRandomGenerator(); }

 private:
  std::unique_ptr<quic::QuicConnectionHelperInterface> helper_;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONTEXT_H_
