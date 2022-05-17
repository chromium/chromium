// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUIC_FLAGS_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUIC_FLAGS_IMPL_H_

#include <cstdint>

#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_export.h"

#define QUIC_PROTOCOL_FLAG(type, flag, ...) \
  QUIC_EXPORT_PRIVATE extern type FLAGS_##flag;
#include "net/third_party/quiche/src/quiche/quic/core/quic_protocol_flags_list.h"
#undef QUIC_PROTOCOL_FLAG

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUIC_FLAGS_IMPL_H_
