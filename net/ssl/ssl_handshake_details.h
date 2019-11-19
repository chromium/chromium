// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_HANDSHAKE_DETAILS_H_
#define NET_SSL_SSL_HANDSHAKE_DETAILS_H_

namespace net {

// This enum is persisted into histograms. Values may not be renumbered.
enum class SSLHandshakeDetails {
  // TLS 1.2 (or earlier) full handshake (2-RTT)
  kTLS12Full = 0,
  // TLS 1.2 (or earlier) resumption (1-RTT)
  kTLS12Resume = 1,
  // TLS 1.2 full handshake with False Start (1-RTT)
  kTLS12FalseStart = 2,
  // 3 was previously used for TLS 1.3 full handshakes with or without HRR.
  // 4 was previously used for TLS 1.3 resumptions with or without HRR.
  // TLS 1.3 0-RTT handshake (0-RTT)
  kTLS13Early = 5,
  // TLS 1.3 full handshake without HelloRetryRequest (1-RTT)
  kTLS13Full = 6,
  // TLS 1.3 resumption handshake without HelloRetryRequest (1-RTT)
  kTLS13Resume = 7,
  // TLS 1.3 full handshake with HelloRetryRequest (2-RTT)
  kTLS13FullWithHelloRetryRequest = 8,
  // TLS 1.3 resumption handshake with HelloRetryRequest (2-RTT)
  kTLS13ResumeWithHelloRetryRequest = 9,
  kMaxValue = kTLS13ResumeWithHelloRetryRequest,
};

}  // namespace net

#endif  // NET_SSL_SSL_HANDSHAKE_DETAILS_H_
