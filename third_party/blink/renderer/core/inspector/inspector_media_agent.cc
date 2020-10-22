// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_media_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_context_impl.h"

#include <utility>

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

namespace {

const char* ConvertMessageLevelEnum(InspectorPlayerMessage::Level level) {
  switch (level) {
    case InspectorPlayerMessage::Level::kError:
      return protocol::Media::PlayerMessage::LevelEnum::Error;
    case InspectorPlayerMessage::Level::kWarning:
      return protocol::Media::PlayerMessage::LevelEnum::Warning;
    case InspectorPlayerMessage::Level::kInfo:
      return protocol::Media::PlayerMessage::LevelEnum::Info;
    case InspectorPlayerMessage::Level::kDebug:
      return protocol::Media::PlayerMessage::LevelEnum::Debug;
  }
}

const char* ConvertErrorTypeEnum(InspectorPlayerError::Type level) {
  switch (level) {
    case InspectorPlayerError::Type::kPipelineError:
      return protocol::Media::PlayerError::TypeEnum::Pipeline_error;
    case InspectorPlayerError::Type::kMediaStatus:
      return protocol::Media::PlayerError::TypeEnum::Media_error;
  }
}

std::unique_ptr<protocol::Media::PlayerEvent> ConvertToProtocolType(
    const InspectorPlayerEvent& event) {
  return protocol::Media::PlayerEvent::create()
      .setTimestamp(event.timestamp.since_origin().InSecondsF())
      .setValue(event.value)
      .build();
}

std::unique_ptr<protocol::Media::PlayerProperty> ConvertToProtocolType(
    const InspectorPlayerProperty& property) {
  return protocol::Media::PlayerProperty::create()
      .setName(property.name)
      .setValue(property.value)
      .build();
}

std::unique_ptr<protocol::Media::PlayerMessage> ConvertToProtocolType(
    const InspectorPlayerMessage& message) {
  return protocol::Media::PlayerMessage::create()
      .setLevel(ConvertMessageLevelEnum(message.level))
      .setMessage(message.message)
      .build();
}

std::unique_ptr<protocol::Media::PlayerError> ConvertToProtocolType(
    const InspectorPlayerError& error) {
  return protocol::Media::PlayerError::create()
      .setType(ConvertErrorTypeEnum(error.type))
      .setErrorCode(error.errorCode)
      .build();
}

template <typename To, typename From>
std::unique_ptr<protocol::Array<To>> ConvertVector(const Vector<From>& from) {
  auto result = std::make_unique<protocol::Array<To>>();
  result->reserve(from.size());
  for (const From& each : from)
    result->push_back(ConvertToProtocolType(each));
  return result;
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
  auto* cache = MediaInspectorContextImpl::From(
      *local_frame_->DomWindow()->GetExecutionContext());
  Vector<WebString> players = cache->AllPlayerIds();
  PlayersCreated(players);
  for (const auto& player_id : players) {
    const auto& media_player = cache->MediaPlayerFromId(player_id);
    Vector<InspectorPlayerProperty> properties;
    properties.AppendRange(media_player.properties.Values().begin(),
                           media_player.properties.Values().end());

    PlayerPropertiesChanged(player_id, properties);
    PlayerMessagesLogged(player_id, media_player.messages);
    PlayerEventsAdded(player_id, media_player.events);
    PlayerErrorsRaised(player_id, media_player.errors);
  }
}

protocol::Response InspectorMediaAgent::enable() {
  if (enabled_.Get())
    return protocol::Response::Success();
  enabled_.Set(true);
  RegisterAgent();
  return protocol::Response::Success();
}

protocol::Response InspectorMediaAgent::disable() {
  if (!enabled_.Get())
    return protocol::Response::Success();
  enabled_.Clear();
  instrumenting_agents_->RemoveInspectorMediaAgent(this);
  return protocol::Response::Success();
}

void InspectorMediaAgent::PlayerPropertiesChanged(
    const WebString& playerId,
    const Vector<InspectorPlayerProperty>& properties) {
  GetFrontend()->playerPropertiesChanged(
      playerId, ConvertVector<protocol::Media::PlayerProperty>(properties));
}

void InspectorMediaAgent::PlayerEventsAdded(
    const WebString& playerId,
    const Vector<InspectorPlayerEvent>& events) {
  GetFrontend()->playerEventsAdded(
      playerId, ConvertVector<protocol::Media::PlayerEvent>(events));
}

void InspectorMediaAgent::PlayerErrorsRaised(
    const WebString& playerId,
    const Vector<InspectorPlayerError>& errors) {
  GetFrontend()->playerErrorsRaised(
      playerId, ConvertVector<protocol::Media::PlayerError>(errors));
}

void InspectorMediaAgent::PlayerMessagesLogged(
    const WebString& playerId,
    const Vector<InspectorPlayerMessage>& messages) {
  GetFrontend()->playerMessagesLogged(
      playerId, ConvertVector<protocol::Media::PlayerMessage>(messages));
}

void InspectorMediaAgent::PlayersCreated(const Vector<WebString>& player_ids) {
  auto protocol_players =
      std::make_unique<protocol::Array<protocol::Media::PlayerId>>();
  protocol_players->reserve(player_ids.size());
  for (const auto& player_id : player_ids)
    protocol_players->push_back(player_id);
  GetFrontend()->playersCreated(std::move(protocol_players));
}

void InspectorMediaAgent::Trace(Visitor* visitor) const {
  visitor->Trace(local_frame_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
