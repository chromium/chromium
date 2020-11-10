// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_PLATFORM_IMPL_SPDY_FLAGS_IMPL_H_
#define NET_SPDY_PLATFORM_IMPL_SPDY_FLAGS_IMPL_H_

#include "net/base/net_export.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"

#define GetSpdyReloadableFlagImpl GetQuicReloadableFlagImpl

#define SPDY_CODE_COUNT_IMPL(name) \
  do {                             \
  } while (0)

#define GetSpdyRestartFlagImpl GetQuicRestartFlagImpl

#define SPDY_CODE_COUNT_N_IMPL(name, instance, total) \
  do {                                                \
  } while (0)

#endif  // NET_SPDY_PLATFORM_IMPL_SPDY_FLAGS_IMPL_H_
