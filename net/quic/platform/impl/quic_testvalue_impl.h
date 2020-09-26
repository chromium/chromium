// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_TESTVALUE_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_TESTVALUE_IMPL_H_

namespace quic {

template <class T>
void AdjustTestValueImpl(quiche::QuicheStringPiece label, T* var) {}

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_TESTVALUE_IMPL_H_
