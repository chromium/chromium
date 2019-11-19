// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_MEDIA_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_MEDIA_AGENT_H_

#include <memory>
#include <vector>

#include "third_party/blink/public/web/web_media_inspector.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/Media.h"

namespace blink {

class CORE_EXPORT InspectorMediaAgent final
    : public InspectorBaseAgent<protocol::Media::Metainfo> {
 public:
  explicit InspectorMediaAgent(InspectedFrames*);
  ~InspectorMediaAgent() override;

  // BaseAgent methods.
  void Restore() override;

  // Protocol receive messages.
  protocol::Response enable() override;
  protocol::Response disable() override;

  // Protocol send messages.
  void PlayerPropertiesChanged(const WebString&,
                               const Vector<InspectorPlayerProperty>&);
  void PlayerEventsAdded(const WebString&, const Vector<InspectorPlayerEvent>&);
  void PlayersCreated(const Vector<WebString>&);

  // blink-gc methods.
  void Trace(blink::Visitor*) override;

 private:
  void RegisterAgent();

  Member<LocalFrame> local_frame_;
  InspectorAgentState::Boolean enabled_;
  DISALLOW_COPY_AND_ASSIGN(InspectorMediaAgent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_MEDIA_AGENT_H_
