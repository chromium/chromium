// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/selector_statistics_flag.h"

#include "base/trace_event/trace_event.h"

namespace blink {

// static
const unsigned char* SelectorStatisticsFlag::GetCategoryGroupEnabled() {
  // Out-of-line to avoid pulling tracing headers absolutely everywhere in
  // Blink, and because this will only be called once per program execution
  // anyway.
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("blink.debug"));
}

}  // namespace blink
