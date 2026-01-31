// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_INSPECTOR_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_INSPECTOR_AGENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/inspector.h"

namespace blink {

class CORE_EXPORT InspectorInspectorAgent final
    : public InspectorBaseAgent<protocol::Inspector::Metainfo> {
 public:
  InspectorInspectorAgent();
  InspectorInspectorAgent(const InspectorInspectorAgent&) = delete;
  InspectorInspectorAgent& operator=(const InspectorInspectorAgent&) = delete;
  ~InspectorInspectorAgent() override;

  // instrumentation
  void WorkerScriptLoaded();

  void Trace(Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_INSPECTOR_AGENT_H_
