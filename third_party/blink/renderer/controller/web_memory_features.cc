// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_memory_features.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/controller/crash_memory_metrics_reporter_impl.h"

namespace blink {

#if BUILDFLAG(IS_ANDROID)
// static
void WebMemoryFeatures::SetRendererVisible(bool visible) {
  CrashMemoryMetricsReporterImpl::Instance().SetRendererVisible(visible);
}
#endif

}  // namespace blink
