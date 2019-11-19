// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/windows_mixed_reality/wrappers/wmr_logging.h"

#include "base/logging.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/win/windows_types.h"

namespace device {
namespace WMRLogging {
void TraceError(WMRErrorLocation location) {
  TRACE_EVENT_INSTANT1("xr", "WMRComError", TRACE_EVENT_SCOPE_THREAD,
                       "ErrorLocation", location);
}

void TraceError(WMRErrorLocation location, HRESULT hr) {
  TRACE_EVENT_INSTANT2("xr", "WMRComError", TRACE_EVENT_SCOPE_THREAD,
                       "ErrorLocation", location, "hr", hr);
}
}  // namespace WMRLogging
}  // namespace device
