// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUIC_FLAGS_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUIC_FLAGS_IMPL_H_

#include <string>

#include "net/third_party/quiche/src/quiche/common/platform/api/quiche_flags.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#define QUIC_PROTOCOL_FLAG(type, flag, ...) \
  QUIC_EXPORT_PRIVATE extern type FLAGS_##flag;
#include "net/third_party/quiche/src/quiche/quic/core/quic_protocol_flags_list.h"
#undef QUIC_PROTOCOL_FLAG

// Sets the flag named |flag_name| to the value of |value| after converting
// it from a string to the appropriate type. If |value| is invalid or out of
// range, the flag will be unchanged.
QUIC_EXPORT_PRIVATE void SetQuicFlagByName(const std::string& flag_name,
                                           const std::string& value);

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUIC_FLAGS_IMPL_H_
