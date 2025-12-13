// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/tracing_delegate.h"

#include "base/functional/bind.h"
#include "components/tracing/common/background_tracing_state_manager.h"

#if BUILDFLAG(IS_WIN)
#include <utility>

#include "base/functional/callback.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

bool TracingDelegate::IsRecordingAllowed(bool requires_anonymized_data,
                                         base::TimeTicks session_start) const {
  return true;
}

bool TracingDelegate::ShouldSaveUnuploadedTrace() const {
  return true;
}

std::unique_ptr<tracing::BackgroundTracingStateManager>
TracingDelegate::CreateStateManager() {
  return nullptr;
}

std::string TracingDelegate::RecordSerializedSystemProfileMetrics() const {
  return std::string();
}

tracing::MetadataDataSource::BundleRecorder
TracingDelegate::CreateSystemProfileMetadataRecorder() const {
  return base::BindRepeating(
      &tracing::MetadataDataSource::RecordDefaultBundleMetadata);
}

#if BUILDFLAG(IS_WIN)
void TracingDelegate::GetSystemTracingState(
    base::OnceCallback<void(bool service_supported, bool service_enabled)>
        on_tracing_state) {
  std::move(on_tracing_state).Run(false, false);
}

void TracingDelegate::EnableSystemTracing(
    base::OnceCallback<void(bool success)> on_complete) {
  std::move(on_complete).Run(false);
}

void TracingDelegate::DisableSystemTracing(
    base::OnceCallback<void(bool success)> on_complete) {
  std::move(on_complete).Run(false);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
