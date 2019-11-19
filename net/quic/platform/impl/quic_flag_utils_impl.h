// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_FLAG_UTILS_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_FLAG_UTILS_IMPL_H_

#include "base/logging.h"
#include "net/quic/platform/impl/quic_flags_impl.h"

#define QUIC_RELOADABLE_FLAG_COUNT_IMPL(flag) \
  DVLOG(3) << "FLAG_" #flag ": " << FLAGS_quic_reloadable_flag_##flag
#define QUIC_RELOADABLE_FLAG_COUNT_N_IMPL(flag, instance, total) \
  QUIC_RELOADABLE_FLAG_COUNT_IMPL(flag)

#define QUIC_RESTART_FLAG_COUNT_IMPL(flag) \
  DVLOG(3) << "FLAG_" #flag ": " << FLAGS_quic_restart_flag_##flag
#define QUIC_RESTART_FLAG_COUNT_N_IMPL(flag, instance, total) \
  QUIC_RESTART_FLAG_COUNT_IMPL(flag)

#define QUIC_CODE_COUNT_IMPL(name) \
  do {                             \
  } while (0)
#define QUIC_CODE_COUNT_N_IMPL(name, instance, total) \
  do {                                                \
  } while (0)

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_FLAG_UTILS_IMPL_H_
