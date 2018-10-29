// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SCOPED_ASYNC_TRACE_H_
#define MEDIA_BASE_SCOPED_ASYNC_TRACE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "media/base/media_export.h"

namespace media {

// Utility class that starts and stops an async trace event.  The intention is
// that it it will be created somewhere to start the trace event, passed around
// such as via unique_ptr argument in a callback, and eventually freed to end
// the trace event.  This guarantees that it'll be closed, even if the callback
// is destroyed without being run.
class MEDIA_EXPORT ScopedAsyncTrace {
 public:
  // Create a ScopedAsyncTrace if tracing for |category| is enabled, else return
  // nullptr.  |name| provided to the trace as the name(!).
  // IMPORTANT: These strings must outlive |this|, since tracing needs it.  In
  // other words, use literal strings only.  See trace_event_common.h .
  static std::unique_ptr<ScopedAsyncTrace> CreateIfEnabled(const char* category,
                                                           const char* name);

  ~ScopedAsyncTrace();

  // TODO(liberato): Add StepInto / StepPast.

 private:
  ScopedAsyncTrace(const char* category, const char* name);

  const char* category_ = nullptr;
  const char* name_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScopedAsyncTrace);
};

}  // namespace media

#endif  // MEDIA_BASE_SCOPED_ASYNC_TRACE_H_
