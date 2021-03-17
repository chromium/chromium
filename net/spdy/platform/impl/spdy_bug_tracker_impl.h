// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_PLATFORM_IMPL_SPDY_BUG_TRACKER_IMPL_H_
#define NET_SPDY_PLATFORM_IMPL_SPDY_BUG_TRACKER_IMPL_H_

#include "base/logging.h"

#define SPDY_BUG_IMPL(bug_id) LOG(DFATAL)
#define SPDY_BUG_IF_IMPL(bug_id, condition) LOG_IF(DFATAL, (condition))

#define SPDY_BUG_V2_IMPL(bug_id) LOG(DFATAL)
#define SPDY_BUG_IF_V2_IMPL(bug_id, condition) LOG_IF(DFATAL, (condition))

#define FLAGS_spdy_always_log_bugs_for_tests_impl (true)

#endif  // NET_SPDY_PLATFORM_IMPL_SPDY_BUG_TRACKER_IMPL_H_
