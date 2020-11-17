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

Vector<WebString> MediaInspectorContextImpl::AllPlayerIdsAndMarkSent() {
  Vector<WebString> existing_players;
  const auto& keys = players_.Keys();
  existing_players.AppendRange(keys.begin(), keys.end());
  unsent_players_.clear();
  return existing_players;
}

const MediaPlayer& MediaInspectorContextImpl::MediaPlayerFromId(
    const WebString& player_id) {
  const auto& player = players_.find(player_id);
  DCHECK_NE(player, players_.end());
  return *player->value;
}

bool MediaInspectorContextImpl::IsConnected() {
  return !!active_session_count_;
}

void MediaInspectorContextImpl::IncrementActiveSessionCount() {
  active_session_count_++;
  DCHECK_GT(active_session_count_, 0lu);
}

void MediaInspectorContextImpl::DecrementActiveSessionCount() {
  active_session_count_--;
}

WebString MediaInspectorContextImpl::CreatePlayer() {
  String next_player_id =
      String::FromUTF8(base::UnguessableToken::Create().ToString());
  players_.insert(next_player_id, MakeGarbageCollected<MediaPlayer>());
  probe::PlayersCreated(GetSupplementable(), {next_player_id});
  if (!IsConnected())
    unsent_players_.push_back(next_player_id);
  return next_player_id;
}

void MediaInspectorContextImpl::RemovePlayer(WebString playerId) {
  const auto& player = players_.find(playerId);
  DCHECK(player != players_.end());
  total_event_count_ -=
      (player->value->errors.size() + player->value->events.size() +
       player->value->messages.size());
  players_.erase(playerId);
}

void MediaInspectorContextImpl::CullPlayers() {
  // Erase all the dead players, but only erase the required number of others.
  for (const auto& playerId : dead_players_)
    RemovePlayer(playerId);
  dead_players_.clear();

  if (total_event_count_ <= kMaxCachedPlayerEvents)
    return;

  for (const auto& playerId : expendable_players_) {
    if (total_event_count_ <= kMaxCachedPlayerEvents)
      return;
    RemovePlayer(playerId);
    expendable_players_.EraseAt(expendable_players_.Find(playerId));
  }

  for (const auto& playerId : unsent_players_) {
    if (total_event_count_ <= kMaxCachedPlayerEvents)
      return;
    RemovePlayer(playerId);
    unsent_players_.EraseAt(unsent_players_.Find(playerId));
  }

  // As a last resort, we'll just remove the first player.
  // TODO(tmathmeyer) keep last event time stamps for players to remove the
  // most stale one.
  for (const auto& playerId : players_.Keys()) {
    if (total_event_count_ <= kMaxCachedPlayerEvents)
      return;
    RemovePlayer(playerId);
  }
}

void MediaInspectorContextImpl::DestroyPlayer(const WebString& playerId) {
  if (unsent_players_.Contains(String(playerId))) {
    // unsent players become dead when destroyed.
    unsent_players_.EraseAt(unsent_players_.Find(String(playerId)));
    dead_players_.push_back(playerId);
    players_.erase(playerId);
  } else {
    expendable_players_.push_back(playerId);
  }
}

// Convert public version of event to protocol version, and send it.
void MediaInspectorContextImpl::NotifyPlayerErrors(
    WebString playerId,
    const InspectorPlayerErrors& errors) {
  const auto& player = players_.find(playerId);
  if (player != players_.end()) {
    player->value->errors.AppendRange(errors.begin(), errors.end());
    total_event_count_ += errors.size();
    if (total_event_count_ > kMaxCachedPlayerEvents)
      CullPlayers();
  }

  Vector<InspectorPlayerError> vector =
      Iter2Vector<InspectorPlayerError>(errors);
  probe::PlayerErrorsRaised(GetSupplementable(), playerId, vector);
}

void MediaInspectorContextImpl::NotifyPlayerEvents(
    WebString playerId,
    const InspectorPlayerEvents& events) {
  const auto& player = players_.find(playerId);
  if (player != players_.end()) {
    player->value->events.AppendRange(events.begin(), events.end());
    total_event_count_ += events.size();
    if (total_event_count_ > kMaxCachedPlayerEvents)
      CullPlayers();
  }

  Vector<InspectorPlayerEvent> vector =
      Iter2Vector<InspectorPlayerEvent>(events);
  probe::PlayerEventsAdded(GetSupplementable(), playerId, vector);
}

void MediaInspectorContextImpl::SetPlayerProperties(
    WebString playerId,
    const InspectorPlayerProperties& props) {
  const auto& player = players_.find(playerId);
  if (player != players_.end()) {
    for (const auto& property : props)
      player->value->properties.insert(property.name, property);
  }

  Vector<InspectorPlayerProperty> vector =
      Iter2Vector<InspectorPlayerProperty>(props);
  probe::PlayerPropertiesChanged(GetSupplementable(), playerId, vector);
}

void MediaInspectorContextImpl::NotifyPlayerMessages(
    WebString playerId,
    const InspectorPlayerMessages& messages) {
  const auto& player = players_.find(playerId);
  if (player != players_.end()) {
    player->value->messages.AppendRange(messages.begin(), messages.end());
    total_event_count_ += messages.size();
    if (total_event_count_ > kMaxCachedPlayerEvents)
      CullPlayers();
  }

  Vector<InspectorPlayerMessage> vector =
      Iter2Vector<InspectorPlayerMessage>(messages);
  probe::PlayerMessagesLogged(GetSupplementable(), playerId, vector);
}

}  // namespace blink
