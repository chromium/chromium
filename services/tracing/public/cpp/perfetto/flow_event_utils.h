// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_FLOW_EVENT_UTILS_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_FLOW_EVENT_UTILS_H_

#include "base/component_export.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_latency_info.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace tracing {

// Fill the information about flow event in EventContext.
//
// BEWARE: this function currently sets the TrackEvent's LegacyEvent field, and
// thus should be used from within trace macros that do not set the LegacyEvent
// field themselves. As it is, it is fine to call this method from the typed
// TRACE_EVENT macro.
//
// TODO(b/147673438): Change to the new model flow events when finalized
void COMPONENT_EXPORT(TRACING_CPP) FillFlowEvent(
    const perfetto::EventContext&,
    perfetto::protos::pbzero::TrackEvent::LegacyEvent::FlowDirection,
    uint64_t bind_id);

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_FLOW_EVENT_UTILS_H_
