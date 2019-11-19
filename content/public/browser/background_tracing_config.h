// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_CONFIG_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_CONFIG_H_

#include <memory>

#include "base/trace_event/trace_event_impl.h"
#include "content/common/content_export.h"

namespace base {
class DictionaryValue;
}

namespace content {

// BackgroundTracingConfig is passed to the BackgroundTracingManager to
// setup the trigger rules used to enable/disable background tracing.
class CONTENT_EXPORT BackgroundTracingConfig {
 public:
  virtual ~BackgroundTracingConfig();

  enum TracingMode {
    PREEMPTIVE,
    REACTIVE,
    // System means that we will inform the system service of triggered rules,
    // but won't manage the trace ourselves.
    SYSTEM,
  };
  TracingMode tracing_mode() const { return tracing_mode_; }

  static std::unique_ptr<BackgroundTracingConfig> FromDict(
      const base::DictionaryValue* dict);

  virtual void IntoDict(base::DictionaryValue* dict) = 0;

 private:
  friend class BackgroundTracingConfigImpl;
  explicit BackgroundTracingConfig(TracingMode tracing_mode);

  const TracingMode tracing_mode_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_CONFIG_H_
