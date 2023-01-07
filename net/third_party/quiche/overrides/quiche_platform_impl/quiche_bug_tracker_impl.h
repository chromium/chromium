// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_BUG_TRACKER_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_BUG_TRACKER_IMPL_H_

#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_logging.h"

#define QUICHE_BUG_IMPL(bug_id) QUIC_LOG(DFATAL)
#define QUICHE_BUG_IF_IMPL(bug_id, condition) QUIC_LOG_IF(DFATAL, condition)
#define QUICHE_PEER_BUG_IMPL(bug_id) QUIC_LOG(ERROR)
#define QUICHE_PEER_BUG_IF_IMPL(bug_id, condition) QUIC_LOG_IF(ERROR, condition)

#define QUICHE_BUG_V2_IMPL(bug_id) QUIC_LOG(DFATAL)
#define QUICHE_BUG_IF_V2_IMPL(bug_id, condition) QUIC_LOG_IF(DFATAL, condition)
#define QUICHE_PEER_BUG_V2_IMPL(bug_id) QUIC_LOG(ERROR)
#define QUICHE_PEER_BUG_IF_V2_IMPL(bug_id, condition) \
  QUIC_LOG_IF(ERROR, condition)

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_BUG_TRACKER_IMPL_H_
