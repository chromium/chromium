// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/global_routing_id.h"

#include "base/pickle.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"

namespace content {

void GlobalRenderFrameHostId::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  proto->set_process_id(child_id);
  proto->set_routing_id(frame_routing_id);
}

base::Pickle GlobalRenderFrameHostToken::ToPickle() {
  base::Pickle pickle;

  pickle.WriteInt(child_id);
  pickle.WriteUInt64(frame_token.value().GetHighForSerialization());
  pickle.WriteUInt64(frame_token.value().GetLowForSerialization());

  return pickle;
}

// static
std::optional<GlobalRenderFrameHostToken>
GlobalRenderFrameHostToken::FromPickle(const base::Pickle& pickle) {
  base::PickleIterator iterator(pickle);
  int child_id = 0;
  uint64_t high = 0;
  uint64_t low = 0;
  if (!iterator.ReadInt(&child_id) || !iterator.ReadUInt64(&high) ||
      !iterator.ReadUInt64(&low)) {
    return std::nullopt;
  }

  auto deserialized_frame_token =
      base::UnguessableToken::Deserialize(high, low);
  if (!deserialized_frame_token) {
    return std::nullopt;
  }

  GlobalRenderFrameHostToken token;

  token.child_id = child_id;
  token.frame_token = blink::LocalFrameToken(*deserialized_frame_token);

  return token;
}

}  // namespace content
