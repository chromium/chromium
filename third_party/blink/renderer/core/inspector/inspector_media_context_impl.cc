// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_media_context_impl.h"

#include <unordered_set>
#include <utility>

#include "base/not_fatal_until.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

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
  WTF::CopyKeysToVector(players_, existing_players);
  unsent_players_.clear();
  return existing_players;
}

const MediaPlayer& MediaInspectorContextImpl::MediaPlayerFromId(
    const WebString& player_id) {
  const auto& player = players_.find(player_id);
  CHECK_NE(player, players_.end(), base::NotFatalUntil::M130);
  return *player->value;
}

WebString MediaInspectorContextImpl::CreatePlayer() {
  String next_player_id =
      String::FromUTF8(base::UnguessableToken::Create().ToString());
  players_.insert(next_player_id, MakeGarbageCollected<MediaPlayer>());
  probe::PlayersCreated(GetSupplementable(), {next_player_id});
  if (!GetSupplementable()->GetProbeSink() ||
      !GetSupplementable()->GetProbeSink()->HasInspectorMediaAgents()) {
    unsent_players_.push_back(next_player_id);
  }
  return next_player_id;
}

void MediaInspectorContextImpl::RemovePlayer(const WebString& playerId) {
  const auto& player = players_.Take(playerId);
  if (player) {
    total_event_count_ -=
        player->errors.size() + player->events.size() + player->messages.size();
    DCHECK_GE(total_event_count_, 0);
  }
}

void MediaInspectorContextImpl::TrimPlayer(const WebString& playerId) {
  MediaPlayer* player = players_.Take(playerId);
  wtf_size_t overage = total_event_count_ - kMaxCachedPlayerEvents;

  wtf_size_t excess = std::min<wtf_size_t>(overage, player->events.size());
  player->events.EraseAt(0, excess);
  total_event_count_ -= excess;
  overage -= excess;

  excess = std::min(overage, player->messages.size());
  player->messages.EraseAt(0, excess);
  total_event_count_ -= excess;
  overage -= excess;

  excess = std::min(overage, player->errors.size());
  player->errors.EraseAt(0, excess);
  total_event_count_ -= excess;
  overage -= excess;

  players_.insert(playerId, player);
}

void MediaInspectorContextImpl::CullPlayers(const WebString& prefer_keep) {
  // Erase all the dead players, but only erase the required number of others.
  while (!dead_players_.empty()) {
    auto playerId = dead_players_.back();
    // remove it first, since |RemovePlayer| can cause a GC event which can
    // potentially caues more players to get added to |dead_players_|.
    dead_players_.pop_back();
    RemovePlayer(playerId);
  }

  while (!expendable_players_.empty()) {
    if (total_event_count_ <= kMaxCachedPlayerEvents)
      return;
    RemovePlayer(expendable_players_.back());
    expendable_players_.pop_back();
  }

  while (!unsent_players_.empty()) {
    if (total_event_count_ <= kMaxCachedPlayerEvents)
      return;
    RemovePlayer(unsent_players_.back());
    unsent_players_.pop_back();
  }

  // TODO(tmathmeyer) keep last event time stamps for players to remove the
  // most stale one.
  while (players_.size() > 1) {
    if (total_event_count_ <= kMaxCachedPlayerEvents)
      return;
    auto iterator = players_.begin();
    // Make sure not to delete the item that is preferred to keep.
    if (WTF::String(prefer_keep) == iterator->key)
      ++iterator;
    RemovePlayer(iterator->key);
  }

  // When there is only one player, selectively remove the oldest events.
  if (players_.size() == 1 && total_event_count_ > kMaxCachedPlayerEvents)
    TrimPlayer(players_.begin()->key);
}

void MediaInspectorContextImpl::DestroyPlayer(const WebString& playerId) {
  if (unsent_players_.Contains(String(playerId))) {
    // unsent players become dead when destroyed.
    unsent_players_.EraseAt(unsent_players_.Find(String(playerId)));
    dead_players_.push_back(playerId);
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
      CullPlayers(playerId);
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
      CullPlayers(playerId);
  }

  Vector<InspectorPlayerEvent> vector =
      Iter2Vector<InspectorPlayerEvent>(events);
  probe::PlayerEventsAdded(GetSupplementable(), playerId, vector);
}

void MediaInspectorContextImpl::SetPlayerProperties(
    WebString playerId,
    const InspectorPlayerProperties& props) {
  const auto& player = players_.find(playerId);
  Vector<InspectorPlayerProperty> properties;
  if (player != players_.end()) {
    for (const auto& property : props)
      player->value->properties.Set(property.name, property);
    WTF::CopyValuesToVector(player->value->properties, properties);
  }
  probe::PlayerPropertiesChanged(GetSupplementable(), playerId, properties);
}

void MediaInspectorContextImpl::NotifyPlayerMessages(
    WebString playerId,
    const InspectorPlayerMessages& messages) {
  const auto& player = players_.find(playerId);
  if (player != players_.end()) {
    player->value->messages.AppendRange(messages.begin(), messages.end());
    total_event_count_ += messages.size();
    if (total_event_count_ > kMaxCachedPlayerEvents)
      CullPlayers(playerId);
  }

  Vector<InspectorPlayerMessage> vector =
      Iter2Vector<InspectorPlayerMessage>(messages);
  probe::PlayerMessagesLogged(GetSupplementable(), playerId, vector);
}

HeapHashMap<String, Member<MediaPlayer>>*
MediaInspectorContextImpl::GetPlayersForTesting() {
  return &players_;
}

}  // namespace blink
