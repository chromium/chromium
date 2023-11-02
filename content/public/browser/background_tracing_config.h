// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_CONFIG_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_CONFIG_H_

#include <memory>

#include "base/trace_event/trace_event_impl.h"
#include "base/values.h"
#include "content/common/content_export.h"

namespace content {

// BackgroundTracingConfig is passed to the BackgroundTracingManager to
// setup the trigger rules used to enable/disable background tracing.
class CONTENT_EXPORT BackgroundTracingConfig {
 public:
  virtual ~BackgroundTracingConfig();

  enum TracingMode {
    PREEMPTIVE = 1 << 0,
    REACTIVE = 1 << 1,
    // System means that we will inform the system service of triggered rules,
    // but won't manage the trace ourselves.
    SYSTEM = 1 << 2,
  };
  TracingMode tracing_mode() const { return tracing_mode_; }

  const std::string& scenario_name() const { return scenario_name_; }
  bool has_crash_scenario() const { return has_crash_scenario_; }

  static std::unique_ptr<BackgroundTracingConfig> FromDict(
      base::Value::Dict&& dict);

  virtual base::Value::Dict ToDict() = 0;

  virtual void SetPackageNameFilteringEnabled(bool) = 0;

 protected:
  std::string scenario_name_;
  bool has_crash_scenario_ = false;

 private:
  friend class BackgroundTracingConfigImpl;
  explicit BackgroundTracingConfig(TracingMode tracing_mode);

  const TracingMode tracing_mode_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_CONFIG_H_
