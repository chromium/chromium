// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TRACING_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_TRACING_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// This can be implemented by the embedder to provide functionality for the
// about://tracing WebUI.
class CONTENT_EXPORT TracingDelegate {
 public:
  virtual ~TracingDelegate() = default;

  // This can be used to veto a particular background tracing scenario.
  virtual bool IsAllowedToBeginBackgroundScenario(
      const std::string& scenario_name,
      bool requires_anonymized_data,
      bool is_crash_scenario);

  virtual bool IsAllowedToEndBackgroundScenario(
      const std::string& scenario_name,
      bool requires_anonymized_data,
      bool is_crash_scenario);

  // Whether system-wide performance trace collection using the external system
  // tracing service is enabled.
  virtual bool IsSystemWideTracingEnabled();

  // Used to add any additional metadata to traces.
  virtual absl::optional<base::Value::Dict> GenerateMetadataDict();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRACING_DELEGATE_H_
