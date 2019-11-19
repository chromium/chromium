// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_BUG_TRACKER_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_BUG_TRACKER_IMPL_H_

#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"

#define QUIC_BUG_IMPL QUIC_LOG(DFATAL)
#define QUIC_BUG_IF_IMPL(condition) QUIC_LOG_IF(DFATAL, condition)
#define QUIC_PEER_BUG_IMPL QUIC_LOG(ERROR)
#define QUIC_PEER_BUG_IF_IMPL(condition) QUIC_LOG_IF(ERROR, condition)

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_BUG_TRACKER_IMPL_H_
