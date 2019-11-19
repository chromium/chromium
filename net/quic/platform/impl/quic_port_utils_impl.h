// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_PORT_UTILS_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_PORT_UTILS_IMPL_H_

namespace quic {

int QuicPickServerPortForTestsOrDieImpl();
void QuicRecyclePortImpl(int port);

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_PORT_UTILS_IMPL_H_
