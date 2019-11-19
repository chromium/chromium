// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_ANIMATION_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_ANIMATION_AGENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/Animation.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8-inspector.h"

namespace blink {

class DocumentTimeline;
class InspectedFrames;
class InspectorCSSAgent;

class CORE_EXPORT InspectorAnimationAgent final
    : public InspectorBaseAgent<protocol::Animation::Metainfo> {
 public:
  InspectorAnimationAgent(InspectedFrames*,
                          InspectorCSSAgent*,
                          v8_inspector::V8InspectorSession*);

  // Base agent methods.
  void Restore() override;
  void DidCommitLoadForLocalFrame(LocalFrame*) override;

  // Protocol method implementations
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response getPlaybackRate(double* playback_rate) override;
  protocol::Response setPlaybackRate(double) override;
  protocol::Response getCurrentTime(const String& id,
                                    double* current_time) override;
  protocol::Response setPaused(
      std::unique_ptr<protocol::Array<String>> animations,
      bool paused) override;
  protocol::Response setTiming(const String& animation_id,
                               double duration,
                               double delay) override;
  protocol::Response seekAnimations(
      std::unique_ptr<protocol::Array<String>> animations,
      double current_time) override;
  protocol::Response releaseAnimations(
      std::unique_ptr<protocol::Array<String>> animations) override;
  protocol::Response resolveAnimation(
      const String& animation_id,
      std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>*)
      override;

  // API for InspectorInstrumentation
  void DidCreateAnimation(unsigned);
  void AnimationPlayStateChanged(blink::Animation*,
                                 blink::Animation::AnimationPlayState,
                                 blink::Animation::AnimationPlayState);
  void DidClearDocumentOfWindowObject(LocalFrame*);

  // Methods for other agents to use.
  protocol::Response AssertAnimation(const String& id,
                                     blink::Animation*& result);

  void Trace(blink::Visitor*) override;

 private:
  using AnimationType = protocol::Animation::Animation::TypeEnum;

  std::unique_ptr<protocol::Animation::Animation> BuildObjectForAnimation(
      blink::Animation&);
  std::unique_ptr<protocol::Animation::Animation> BuildObjectForAnimation(
      blink::Animation&,
      String,
      std::unique_ptr<protocol::Animation::KeyframesRule> keyframe_rule =
          nullptr);
  double NormalizedStartTime(blink::Animation&);
  DocumentTimeline& ReferenceTimeline();
  blink::Animation* AnimationClone(blink::Animation*);
  String CreateCSSId(blink::Animation&);

  Member<InspectedFrames> inspected_frames_;
  Member<InspectorCSSAgent> css_agent_;
  v8_inspector::V8InspectorSession* v8_session_;
  HeapHashMap<String, Member<blink::Animation>> id_to_animation_;
  HeapHashMap<String, Member<blink::Animation>> id_to_animation_clone_;
  bool is_cloning_;
  HashSet<String> cleared_animations_;
  InspectorAgentState::Boolean enabled_;
  InspectorAgentState::Double playback_rate_;
  DISALLOW_COPY_AND_ASSIGN(InspectorAnimationAgent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_ANIMATION_AGENT_H_
