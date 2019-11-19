// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_media_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_context_impl.h"

#include <utility>

#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

namespace {

std::unique_ptr<protocol::Media::PlayerEvent> ConvertInspectorPlayerEvent(
    const InspectorPlayerEvent& event) {
  protocol::Media::PlayerEventType event_type;
  switch (event.type) {
    case InspectorPlayerEvent::PLAYBACK_EVENT:
      event_type = protocol::Media::PlayerEventTypeEnum::PlaybackEvent;
      break;
    case InspectorPlayerEvent::SYSTEM_EVENT:
      event_type = protocol::Media::PlayerEventTypeEnum::SystemEvent;
      break;
    case InspectorPlayerEvent::MESSAGE_EVENT:
      event_type = protocol::Media::PlayerEventTypeEnum::MessageEvent;
      break;
  }
  return protocol::Media::PlayerEvent::create()
      .setType(event_type)
      .setTimestamp(event.timestamp.since_origin().InSecondsF())
      .setName(event.key)
      .setValue(event.value)
      .build();
}

std::unique_ptr<protocol::Media::PlayerProperty> ConvertInspectorPlayerProperty(
    const InspectorPlayerProperty& property) {
  auto builder = std::move(
      protocol::Media::PlayerProperty::create().setName(property.name));
  if (property.value.has_value())
    builder.setValue(property.value.value());
  return builder.build();
}

}  // namespace

InspectorMediaAgent::InspectorMediaAgent(InspectedFrames* inspected_frames)
    : local_frame_(inspected_frames->Root()),
      enabled_(&agent_state_, /*default_value = */ false) {}

InspectorMediaAgent::~InspectorMediaAgent() = default;

void InspectorMediaAgent::Restore() {
  if (!enabled_.Get())
    return;
  RegisterAgent();
}

void InspectorMediaAgent::RegisterAgent() {
  instrumenting_agents_->AddInspectorMediaAgent(this);
  auto* cache = MediaInspectorContextImpl::FromLocalFrame(local_frame_);
  Vector<WebString> players = cache->GetAllPlayerIds();
  PlayersCreated(players);
  for (const auto& player_id : players) {
    auto props_events = cache->GetPropertiesAndEvents(player_id);
    PlayerPropertiesChanged(player_id, props_events.first);
    PlayerEventsAdded(player_id, props_events.second);
  }
}

protocol::Response InspectorMediaAgent::enable() {
  if (enabled_.Get())
    return protocol::Response::OK();
  enabled_.Set(true);
  RegisterAgent();
  return protocol::Response::OK();
}

protocol::Response InspectorMediaAgent::disable() {
  if (!enabled_.Get())
    return protocol::Response::OK();
  enabled_.Clear();
  instrumenting_agents_->RemoveInspectorMediaAgent(this);
  return protocol::Response::OK();
}

void InspectorMediaAgent::PlayerPropertiesChanged(
    const WebString& playerId,
    const Vector<InspectorPlayerProperty>& properties) {
  auto protocol_props =
      std::make_unique<protocol::Array<protocol::Media::PlayerProperty>>();
  protocol_props->reserve(properties.size());
  for (const auto& property : properties)
    protocol_props->push_back(ConvertInspectorPlayerProperty(property));
  GetFrontend()->playerPropertiesChanged(playerId, std::move(protocol_props));
}

void InspectorMediaAgent::PlayerEventsAdded(
    const WebString& playerId,
    const Vector<InspectorPlayerEvent>& events) {
  auto protocol_events =
      std::make_unique<protocol::Array<protocol::Media::PlayerEvent>>();
  protocol_events->reserve(events.size());
  for (const auto& event : events)
    protocol_events->push_back(ConvertInspectorPlayerEvent(event));
  GetFrontend()->playerEventsAdded(playerId, std::move(protocol_events));
}

void InspectorMediaAgent::PlayersCreated(const Vector<WebString>& player_ids) {
  auto protocol_players =
      std::make_unique<protocol::Array<protocol::Media::PlayerId>>();
  protocol_players->reserve(player_ids.size());
  for (const auto& player_id : player_ids)
    protocol_players->push_back(player_id);
  GetFrontend()->playersCreated(std::move(protocol_players));
}

void InspectorMediaAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(local_frame_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
