// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_TRIGGERS_DATA_SOURCE_H_
#define SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_TRIGGERS_DATA_SOURCE_H_

#include "base/component_export.h"
#include "services/tracing/public/cpp/background_tracing/background_tracing_rule.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"

namespace tracing {

class COMPONENT_EXPORT(BACKGROUND_TRACING_CPP) TriggersDataSource
    : public perfetto::DataSource<TriggersDataSource> {
 public:
  static void Register();
  static void EmitTrigger(const tracing::BackgroundTracingRule* triggered_rule);

  void OnStart(const StartArgs&) override;
  void OnStop(const StopArgs&) override;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_BACKGROUND_TRACING_TRIGGERS_DATA_SOURCE_H_
