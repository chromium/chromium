// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/tracing_delegate.h"

#if BUILDFLAG(IS_WIN)
#include <utility>

#include "base/functional/callback.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

bool TracingDelegate::IsRecordingAllowed(bool requires_anonymized_data) const {
  return false;
}

bool TracingDelegate::ShouldSaveUnuploadedTrace() const {
  return false;
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
