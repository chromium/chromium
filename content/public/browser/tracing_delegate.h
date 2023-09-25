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

  // Notifies that background tracing became active and a tracing session
  // started. Returns true if the tracing session is allowed to begin.
  virtual bool OnBackgroundTracingActive(bool requires_anonymized_data);
  // Notifies that a tracing session stopped and background tracing became idle
  // again. Returns true if the tracing session is allowed finalize.
  virtual bool OnBackgroundTracingIdle(bool requires_anonymized_data);

  // Specifies whether traces that aren't uploaded should still be saved.
  virtual bool ShouldSaveUnuploadedTrace() const;

  // Whether system-wide performance trace collection using the external system
  // tracing service is enabled.
  virtual bool IsSystemWideTracingEnabled();

  // Used to add any additional metadata to traces.
  virtual absl::optional<base::Value::Dict> GenerateMetadataDict();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRACING_DELEGATE_H_
