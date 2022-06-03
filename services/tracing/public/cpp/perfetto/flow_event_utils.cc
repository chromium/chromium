// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/flow_event_utils.h"

namespace tracing {

// Fill the information about flow event in EventContext.
//
// BEWARE: this function currently sets the TrackEvent's LegacyEvent field, and
// thus should be used from within trace macros that do not set the LegacyEvent
// field themselves. As it is, it is fine to call this method from the typed
// TRACE_EVENT macro.
//
// TODO(b/TODO): Change to the new model flow events when finalized
void FillFlowEvent(
    const perfetto::EventContext& ctx,
    perfetto::protos::pbzero::TrackEvent_LegacyEvent_FlowDirection direction,
    uint64_t bind_id) {
  perfetto::protos::pbzero::TrackEvent_LegacyEvent* legacy_event =
      ctx.event()->set_legacy_event();
  legacy_event->set_flow_direction(direction);
  legacy_event->set_bind_id(bind_id);
}

}  // namespace tracing