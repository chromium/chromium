// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/inspector_helper_mixin.h"

#include "third_party/blink/renderer/platform/wtf/uuid.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"

namespace blink {

InspectorHelperMixin::InspectorHelperMixin(
    AudioGraphTracer& graph_tracer, const String& parent_uuid)
    : graph_tracer_(graph_tracer),
      uuid_(WTF::CreateCanonicalUUIDString()),
      parent_uuid_(parent_uuid) {}

void InspectorHelperMixin::Trace(blink::Visitor* visitor) {
  visitor->Trace(graph_tracer_);
}

}  // namespace blink
