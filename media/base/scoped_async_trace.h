// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SCOPED_ASYNC_TRACE_H_
#define MEDIA_BASE_SCOPED_ASYNC_TRACE_H_

#include <memory>

#include "media/base/media_export.h"

namespace media {

// Utility class that starts and stops an async trace event.  The intention is
// that it it will be created somewhere to start the trace event, passed around
// such as via unique_ptr argument in a callback, and eventually freed to end
// the trace event.  This guarantees that it'll be closed, even if the callback
// is destroyed without being run.
class MEDIA_EXPORT ScopedAsyncTrace {
 public:
  // Create a ScopedAsyncTrace if tracing for "media" is enabled, else return
  // nullptr.  |name| provided to the trace as the name(!).
  // IMPORTANT: These strings must outlive |this|, since tracing needs it.  In
  // other words, use literal strings only.  See trace_event_common.h .
  static std::unique_ptr<ScopedAsyncTrace> CreateIfEnabled(const char* name);

  ScopedAsyncTrace(const ScopedAsyncTrace&) = delete;
  ScopedAsyncTrace& operator=(const ScopedAsyncTrace&) = delete;

  ~ScopedAsyncTrace();

  // TODO(liberato): Add StepInto / StepPast.

 private:
  explicit ScopedAsyncTrace(const char* name);

  const char* name_ = nullptr;
};

}  // namespace media

#endif  // MEDIA_BASE_SCOPED_ASYNC_TRACE_H_
