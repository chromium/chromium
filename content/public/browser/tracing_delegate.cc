// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/tracing_delegate.h"

#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

bool TracingDelegate::IsAllowedToBeginBackgroundScenario(
    const BackgroundTracingConfig& config,
    bool requires_anonymized_data) {
  return false;
}

bool TracingDelegate::IsAllowedToEndBackgroundScenario(
    const content::BackgroundTracingConfig& config,
    bool requires_anonymized_data,
    bool is_crash_scenario) {
  return false;
}

bool TracingDelegate::IsSystemWideTracingEnabled() {
  return false;
}

absl::optional<base::Value::Dict> TracingDelegate::GenerateMetadataDict() {
  return absl::nullopt;
}

}  // namespace content
