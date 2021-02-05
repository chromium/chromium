// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/tracing_delegate.h"

#include "base/values.h"

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

bool TracingDelegate::IsProfileLoaded() {
  return false;
}

bool TracingDelegate::IsSystemWideTracingEnabled() {
  return false;
}

std::unique_ptr<base::DictionaryValue> TracingDelegate::GenerateMetadataDict() {
  return nullptr;
}

}  // namespace content
