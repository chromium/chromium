// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_inspector_agent.h"

#include "third_party/blink/renderer/core/workers/worker_global_scope.h"

namespace blink {

InspectorInspectorAgent::InspectorInspectorAgent() = default;

InspectorInspectorAgent::~InspectorInspectorAgent() = default;

void InspectorInspectorAgent::WorkerScriptLoaded() {
  GetFrontend()->workerScriptLoaded();
}

void InspectorInspectorAgent::Trace(Visitor* visitor) const {
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
