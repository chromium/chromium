// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACES_INTERNALS_TRACES_INTERNALS_MOJOM_TRAITS_H_
#define CONTENT_BROWSER_TRACING_TRACES_INTERNALS_TRACES_INTERNALS_MOJOM_TRAITS_H_

#include "content/browser/tracing/traces_internals/traces_internals.mojom.h"
#include "services/tracing/public/cpp/background_tracing/trace_report_database.h"
#include "services/tracing/public/cpp/background_tracing/tracing_scenario.h"

namespace mojo {

using ReportUploadState = traces_internals::mojom::ReportUploadState;
using SkipUploadReason = traces_internals::mojom::SkipUploadReason;
using TracingScenarioState = traces_internals::mojom::TracingScenarioState;

template <>
struct EnumTraits<ReportUploadState, tracing::ReportUploadState> {
  static ReportUploadState ToMojom(tracing::ReportUploadState input);
  static tracing::ReportUploadState FromMojom(ReportUploadState input);
};

template <>
struct EnumTraits<SkipUploadReason, tracing::SkipUploadReason> {
  static SkipUploadReason ToMojom(tracing::SkipUploadReason input);
  static tracing::SkipUploadReason FromMojom(SkipUploadReason input);
};

template <>
struct EnumTraits<TracingScenarioState, tracing::TracingScenario::State> {
  static TracingScenarioState ToMojom(tracing::TracingScenario::State input);
  static tracing::TracingScenario::State FromMojom(TracingScenarioState input);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_TRACING_TRACES_INTERNALS_TRACES_INTERNALS_MOJOM_TRAITS_H_
