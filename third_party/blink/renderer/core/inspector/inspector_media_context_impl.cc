// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_media_context_impl.h"

#include <unordered_set>
#include <utility>

#include "base/unguessable_token.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

const char MediaInspectorContextImpl::kSupplementName[] =
    "MediaInspectorContextImpl";


// static
MediaInspectorContextImpl* MediaInspectorContextImpl::From(
    ExecutionContext& execution_context) {
  auto* context = Supplement<ExecutionContext>::From<MediaInspectorContextImpl>(
      execution_context);
  if (!context) {
    context =
        MakeGarbageCollected<MediaInspectorContextImpl>(execution_context);
    Supplement<ExecutionContext>::ProvideTo(execution_context, context);
  }
  return context;
}

MediaInspectorContextImpl::MediaInspectorContextImpl(ExecutionContext& context)
    : Supplement<ExecutionContext>(context) {
  DCHECK(context.IsWindow() || context.IsWorkerGlobalScope());
}

// Local to cc file for converting
template <typename T, typename Iterable>
static Vector<T> Iter2Vector(const Iterable& iterable) {
  Vector<T> result;
  result.AppendRange(iterable.begin(), iterable.end());
  return result;
}

// Garbage collection method.
void MediaInspectorContextImpl::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
  visitor->Trace(players_);
}

Vector<WebString> MediaInspectorContextImpl::AllPlayerIds() {
  Vector<WebString> existing_players;
  existing_players.ReserveCapacity(players_.size());
  for (const auto& player_id : players_.Keys())
    existing_players.push_back(player_id);
  return existing_players;
}

const MediaPlayer& MediaInspectorContextImpl::MediaPlayerFromId(
    const WebString& player_id) {
  const auto& player = players_.find(player_id);
  DCHECK_NE(player, players_.end());
  return *player->value;
}

WebString MediaInspectorContextImpl::CreatePlayer() {
  String next_player_id =
      String::FromUTF8(base::UnguessableToken::Create().ToString());
  players_.insert(next_player_id, MakeGarbageCollected<MediaPlayer>());
  probe::PlayersCreated(GetSupplementable(), {next_player_id});
  return next_player_id;
}

// Convert public version of event to protocol version, and send it.
void MediaInspectorContextImpl::NotifyPlayerErrors(
    WebString playerId,
    const InspectorPlayerErrors& errors) {
  const auto& player = players_.find(playerId);
  DCHECK_NE(player, players_.end());
  player->value->errors.AppendRange(errors.begin(), errors.end());

  Vector<InspectorPlayerError> vector =
      Iter2Vector<InspectorPlayerError>(errors);
  probe::PlayerErrorsRaised(GetSupplementable(), playerId, vector);
}

void MediaInspectorContextImpl::NotifyPlayerEvents(
    WebString playerId,
    const InspectorPlayerEvents& events) {
  const auto& player = players_.find(playerId);
  DCHECK_NE(player, players_.end());
  player->value->events.AppendRange(events.begin(), events.end());

  Vector<InspectorPlayerEvent> vector =
      Iter2Vector<InspectorPlayerEvent>(events);
  probe::PlayerEventsAdded(GetSupplementable(), playerId, vector);
}

void MediaInspectorContextImpl::SetPlayerProperties(
    WebString playerId,
    const InspectorPlayerProperties& props) {
  const auto& player = players_.find(playerId);
  DCHECK_NE(player, players_.end());
  for (const auto& property : props)
    player->value->properties.insert(property.name, property);

  Vector<InspectorPlayerProperty> vector =
      Iter2Vector<InspectorPlayerProperty>(props);
  probe::PlayerPropertiesChanged(GetSupplementable(), playerId, vector);
}

void MediaInspectorContextImpl::NotifyPlayerMessages(
    WebString playerId,
    const InspectorPlayerMessages& messages) {
  const auto& player = players_.find(playerId);
  DCHECK_NE(player, players_.end());
  player->value->messages.AppendRange(messages.begin(), messages.end());

  Vector<InspectorPlayerMessage> vector =
      Iter2Vector<InspectorPlayerMessage>(messages);
  probe::PlayerMessagesLogged(GetSupplementable(), playerId, vector);
}

}  // namespace blink
