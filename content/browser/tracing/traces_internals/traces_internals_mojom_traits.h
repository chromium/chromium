// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACES_INTERNALS_TRACES_INTERNALS_MOJOM_TRAITS_H_
#define CONTENT_BROWSER_TRACING_TRACES_INTERNALS_TRACES_INTERNALS_MOJOM_TRAITS_H_

#include "content/browser/tracing/trace_report_database.h"
#include "content/browser/tracing/traces_internals/traces_internals.mojom.h"
#include "content/browser/tracing/tracing_scenario.h"

namespace mojo {

using ReportUploadState = traces_internals::mojom::ReportUploadState;
using SkipUploadReason = traces_internals::mojom::SkipUploadReason;
using TracingScenarioState = traces_internals::mojom::TracingScenarioState;

template <>
struct EnumTraits<ReportUploadState, content::ReportUploadState> {
  static ReportUploadState ToMojom(content::ReportUploadState input);
  static bool FromMojom(ReportUploadState input,
                        content::ReportUploadState* output);
};

template <>
struct EnumTraits<SkipUploadReason, content::SkipUploadReason> {
  static SkipUploadReason ToMojom(content::SkipUploadReason input);
  static bool FromMojom(SkipUploadReason input,
                        content::SkipUploadReason* output);
};

template <>
struct EnumTraits<TracingScenarioState, content::TracingScenario::State> {
  static TracingScenarioState ToMojom(content::TracingScenario::State input);
  static bool FromMojom(TracingScenarioState input,
                        content::TracingScenario::State* output);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_TRACING_TRACES_INTERNALS_TRACES_INTERNALS_MOJOM_TRAITS_H_
