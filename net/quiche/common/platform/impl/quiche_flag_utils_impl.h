// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_FLAG_UTILS_IMPL_H_
#define NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_FLAG_UTILS_IMPL_H_

#include "base/logging.h"

#define QUICHE_RELOADABLE_FLAG_COUNT_IMPL(flag) \
  DVLOG(3) << "FLAG_" #flag ": reloadable"
#define QUICHE_RELOADABLE_FLAG_COUNT_N_IMPL(flag, instance, total) \
  DVLOG(3) << "FLAG_" #flag ": reloadable, instance: " << instance \
           << " total: " << total

#define QUICHE_RESTART_FLAG_COUNT_IMPL(flag) \
  DVLOG(3) << "FLAG_" #flag ": resart"
#define QUICHE_RESTART_FLAG_COUNT_N_IMPL(flag, instance, total) \
  DVLOG(3) << "FLAG_" #flag ": restart, instance: " << instance \
           << " total: " << total

#define QUICHE_CODE_COUNT_IMPL(flag) DVLOG(3) << "FLAG_" #flag
#define QUICHE_CODE_COUNT_N_IMPL(flag, instance, total) \
  DVLOG(3) << "FLAG_" #flag

#endif  // NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_FLAG_UTILS_IMPL_H_
