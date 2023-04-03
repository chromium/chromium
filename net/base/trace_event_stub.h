// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines stubs for tracing-related classes which are used by net
// but are not available in Cronet builds (where tracing is disabled to reduce
// binary size).
// Even though these stubs are part of the base::trace_event namespace, they're
// technically not required by base. So, implement them here to make that
// explicit.

#ifndef NET_BASE_TRACE_EVENT_STUB_H_
#define NET_BASE_TRACE_EVENT_STUB_H_

#import "base/base_export.h"
#import "base/memory/weak_ptr.h"
#import "base/trace_event/trace_event_stub.h"

namespace base::trace_event {

class BASE_EXPORT TraceLog {
 public:
  class BASE_EXPORT AsyncEnabledStateObserver {
   public:
    virtual ~AsyncEnabledStateObserver() = default;

    virtual void OnTraceLogEnabled() = 0;
    virtual void OnTraceLogDisabled() = 0;
  };

  static TraceLog* GetInstance() {
    static TraceLog obj;
    return &obj;
  }

  bool IsEnabled() { return false; }

  void AddAsyncEnabledStateObserver(WeakPtr<AsyncEnabledStateObserver>) {}
  void RemoveAsyncEnabledStateObserver(AsyncEnabledStateObserver*) {}
};

}  // namespace base::trace_event

#endif  // NET_BASE_TRACE_EVENT_STUB_H_
