// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRIGGERS_DATA_SOURCE_H_
#define CONTENT_BROWSER_TRACING_TRIGGERS_DATA_SOURCE_H_

#include "content/browser/tracing/background_tracing_rule.h"
#include "content/common/content_export.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"

namespace content {

class CONTENT_EXPORT TriggersDataSource
    : public perfetto::DataSource<TriggersDataSource> {
 public:
  static void Register();
  static void EmitTrigger(const BackgroundTracingRule* triggered_rule);

  void OnStart(const StartArgs&) override;
  void OnStop(const StopArgs&) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRIGGERS_DATA_SOURCE_H_
