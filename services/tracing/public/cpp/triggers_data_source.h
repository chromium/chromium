// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRIGGERS_DATA_SOURCE_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRIGGERS_DATA_SOURCE_H_

#include <string>

#include "base/component_export.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"

namespace tracing {

class COMPONENT_EXPORT(TRACING_CPP) TriggersDataSource
    : public perfetto::DataSource<TriggersDataSource> {
 public:
  static void Register();
  static void EmitTrigger(const std::string& trigger_name);

  void OnStart(const StartArgs&) override;
  void OnStop(const StopArgs&) override;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_TRIGGERS_DATA_SOURCE_H_
