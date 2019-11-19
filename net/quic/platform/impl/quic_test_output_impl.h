// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_TEST_OUTPUT_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_TEST_OUTPUT_IMPL_H_

#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"

namespace quic {

void QuicRecordTestOutputImpl(QuicStringPiece identifier, QuicStringPiece data);

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_TEST_OUTPUT_IMPL_H_
