// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TRACING_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_TRACING_DELEGATE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "services/tracing/public/cpp/perfetto/metadata_data_source.h"

namespace tracing {
class BackgroundTracingStateManager;
}

namespace content {

// This can be implemented by the embedder to provide functionality for the
// about://tracing WebUI.
class CONTENT_EXPORT TracingDelegate {
 public:
  virtual ~TracingDelegate() = default;

  // Returns true if a tracing session that started at `tracing_start` is
  // allowed to record or save data.
  virtual bool IsRecordingAllowed(bool requires_anonymized_data,
                                  base::TimeTicks session_start) const;

  // Specifies whether traces that aren't uploaded should still be saved.
  virtual bool ShouldSaveUnuploadedTrace() const;

  // Creates background tracing state manager connected to embedder local state.
  virtual std::unique_ptr<tracing::BackgroundTracingStateManager>
  CreateStateManager();

  // Returns a serialized system metrics.
  virtual std::string RecordSerializedSystemProfileMetrics() const;

  // Returns a callback to fill a bundle packet with system metrics.
  virtual tracing::MetadataDataSource::BundleRecorder
  CreateSystemProfileMetadataRecorder() const;

#if BUILDFLAG(IS_WIN)
  // Runs `on_tracing_state` (asynchronously) with the current state of the
  // Windows system tracing service for the running browser:
  // - `service_supported`: true if the service is supported.
  // - `service_enabled`: true if the service is enabled.
  virtual void GetSystemTracingState(
      base::OnceCallback<void(bool service_supported, bool service_enabled)>
          on_tracing_state);

  // Enables the Windows system tracing service and runs `on_complete` with
  // the result of the operation. The user must pass a UAC prompt to enable the
  // service.
  virtual void EnableSystemTracing(
      base::OnceCallback<void(bool success)> on_complete);

  // Disables the Windows system tracing service and runs `on_complete` with
  // the result of the operation. The user must pass a UAC prompt to disable the
  // service.
  virtual void DisableSystemTracing(
      base::OnceCallback<void(bool success)> on_complete);
#endif  // BUILDFLAG(IS_WIN)
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRACING_DELEGATE_H_
