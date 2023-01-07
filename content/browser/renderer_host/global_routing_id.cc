// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/global_routing_id.h"

#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"

namespace content {

void GlobalRenderFrameHostId::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  proto->set_process_id(child_id);
  proto->set_routing_id(frame_routing_id);
}

}  // namespace content
