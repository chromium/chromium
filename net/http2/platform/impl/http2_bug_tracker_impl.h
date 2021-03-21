// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP2_PLATFORM_IMPL_HTTP2_BUG_TRACKER_IMPL_H_
#define NET_HTTP2_PLATFORM_IMPL_HTTP2_BUG_TRACKER_IMPL_H_

#include "base/logging.h"

#define HTTP2_BUG_IMPL(bug_id) LOG(DFATAL)
#define HTTP2_BUG_IF_IMPL(bug_id) LOG_IF(DFATAL, (condition))

#define HTTP2_BUG_V2_IMPL(bug_id) LOG(DFATAL)
#define HTTP2_BUG_IF_V2_IMPL(bug_id, condition) LOG_IF(DFATAL, (condition))

#define FLAGS_http2_always_log_bugs_for_tests_IMPL (true)

#endif  // NET_HTTP2_PLATFORM_IMPL_HTTP2_BUG_TRACKER_IMPL_H_
