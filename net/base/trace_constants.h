// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TRACE_CONSTANTS_H_
#define NET_BASE_TRACE_CONSTANTS_H_

#include "base/trace_event/common/trace_event_common.h"

namespace net {

// Net Category used in Tracing.
constexpr const char* NetTracingCategory() {
  // Declared as a constexpr function to have an external linkage and to be
  // known at compile-time.
  return TRACE_DISABLED_BY_DEFAULT("net");
}

}  // namespace net

#endif  // NET_BASE_TRACE_CONSTANTS_H_
