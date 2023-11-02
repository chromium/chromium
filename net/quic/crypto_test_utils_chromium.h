// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_TEST_UTILS_CHROMIUM_H_
#define NET_QUIC_CRYPTO_TEST_UTILS_CHROMIUM_H_

#include <memory>

#include "net/third_party/quiche/src/quiche/quic/core/crypto/proof_source.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/proof_verifier.h"

namespace net::test {

std::unique_ptr<quic::ProofSource> ProofSourceForTestingChromium();

}  // namespace net::test

#endif  // NET_QUIC_CRYPTO_TEST_UTILS_CHROMIUM_H_
