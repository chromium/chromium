// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_media_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_context_impl.h"

#include <utility>

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"

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

std::unique_ptr<protocol::Media::PlayerErrorSourceLocation>
ConvertToProtocolType(const InspectorPlayerError::SourceLocation& stack) {
  return protocol::Media::PlayerErrorSourceLocation::create()
      .setFile(stack.filename)
      .setLine(stack.line_number)
      .build();
}

std::unique_ptr<protocol::Media::PlayerError> ConvertToProtocolType(
    const InspectorPlayerError& error) {
  auto caused_by =
      std::make_unique<protocol::Array<protocol::Media::PlayerError>>();
  auto stack = std::make_unique<
      protocol::Array<protocol::Media::PlayerErrorSourceLocation>>();
  auto data = protocol::DictionaryValue::create();

  for (const InspectorPlayerError& cause : error.caused_by)
    caused_by->push_back(ConvertToProtocolType(cause));

  for (const InspectorPlayerError::Data& pair : error.data)
    data->setString(pair.name, pair.value);

  for (const InspectorPlayerError::SourceLocation& pair : error.stack)
    stack->push_back(ConvertToProtocolType(pair));

  return protocol::Media::PlayerError::create()
      .setErrorType(error.group)
      .setCode(error.code)
      .setCause(std::move(caused_by))
      .setData(std::move(data))
      .setStack(std::move(stack))
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

InspectorMediaAgent::InspectorMediaAgent(InspectedFrames* inspected_frames,
                                         WorkerGlobalScope* worker_global_scope)
    : inspected_frames_(inspected_frames),
      worker_global_scope_(worker_global_scope),
      enabled_(&agent_state_, /* default_value = */ false) {}

InspectorMediaAgent::~InspectorMediaAgent() = default;

ExecutionContext* InspectorMediaAgent::GetTargetExecutionContext() const {
  if (worker_global_scope_)
    return worker_global_scope_.Get();
  DCHECK(inspected_frames_);
  return inspected_frames_->Root()->DomWindow()->GetExecutionContext();
}

void InspectorMediaAgent::Restore() {
  if (!enabled_.Get())
    return;
  RegisterAgent();
}

void InspectorMediaAgent::RegisterAgent() {
  instrumenting_agents_->AddInspectorMediaAgent(this);
  auto* cache = MediaInspectorContextImpl::From(*GetTargetExecutionContext());
  Vector<WebString> players = cache->AllPlayerIdsAndMarkSent();
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
  visitor->Trace(inspected_frames_);
  visitor->Trace(worker_global_scope_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
