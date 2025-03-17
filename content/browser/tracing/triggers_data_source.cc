// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/triggers_data_source.h"

#include "base/metrics/metrics_hashes.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/trace_time.h"
#include "third_party/perfetto/protos/perfetto/common/data_source_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trigger.pbzero.h"

namespace content {

void TriggersDataSource::Register() {
  perfetto::DataSourceDescriptor desc;
  desc.set_name("org.chromium.triggers");
  CHECK(perfetto::DataSource<TriggersDataSource>::Register(desc));
}

void TriggersDataSource::EmitTrigger(
    const BackgroundTracingRule* triggered_rule) {
  Trace([triggered_rule](TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(
        TRACE_TIME_TICKS_NOW().since_origin().InNanoseconds());
    packet->set_timestamp_clock_id(base::tracing::kTraceClockId);
    auto* trigger = packet->set_chrome_trigger();
    trigger->set_trigger_name(triggered_rule->rule_name());
    trigger->set_trigger_name_hash(
        base::HashFieldTrialName(triggered_rule->rule_name()));
    trigger->set_flow_id(triggered_rule->flow_id());
  });
}

void TriggersDataSource::OnStart(const StartArgs&) {}
void TriggersDataSource::OnStop(const StopArgs&) {}

}  // namespace content

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(
    CONTENT_EXPORT,
    content::TriggersDataSource);
