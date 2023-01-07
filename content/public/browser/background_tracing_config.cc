// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/background_tracing_config.h"

#include <utility>

#include "content/browser/tracing/background_tracing_config_impl.h"

namespace content {

BackgroundTracingConfig::BackgroundTracingConfig(TracingMode tracing_mode)
    : tracing_mode_(tracing_mode) {}

BackgroundTracingConfig::~BackgroundTracingConfig() {}

std::unique_ptr<BackgroundTracingConfig> BackgroundTracingConfig::FromDict(
    base::Value::Dict&& dict) {
  return BackgroundTracingConfigImpl::FromDict(std::move(dict));
}

}  // namespace content
