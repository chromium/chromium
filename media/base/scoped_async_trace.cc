// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/scoped_async_trace.h"

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"

namespace media {

namespace {
constexpr const char kCategory[] = "media";
}  // namespace

// static
std::unique_ptr<ScopedAsyncTrace> ScopedAsyncTrace::CreateIfEnabled(
    const char* name) {
  bool enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kCategory, &enabled);
  return enabled ? base::WrapUnique(new ScopedAsyncTrace(name)) : nullptr;
}

ScopedAsyncTrace::ScopedAsyncTrace(const char* name) : name_(name) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kCategory, name_, TRACE_ID_LOCAL(this));
}

ScopedAsyncTrace::~ScopedAsyncTrace() {
  TRACE_EVENT_NESTABLE_ASYNC_END0(kCategory, name_, TRACE_ID_LOCAL(this));
}

}  // namespace media
