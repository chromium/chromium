// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/scoped_async_trace.h"

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"

namespace media {

// static
std::unique_ptr<ScopedAsyncTrace> ScopedAsyncTrace::CreateIfEnabled(
    const char* category,
    const char* name) {
  bool enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(category, &enabled);
  return enabled ? base::WrapUnique(new ScopedAsyncTrace(category, name))
                 : nullptr;
}

ScopedAsyncTrace::ScopedAsyncTrace(const char* category, const char* name)
    : category_(category), name_(name) {
  TRACE_EVENT_ASYNC_BEGIN0(category_, name_, this);
}

ScopedAsyncTrace::~ScopedAsyncTrace() {
  TRACE_EVENT_ASYNC_END0(category_, name_, this);
}

}  // namespace media
