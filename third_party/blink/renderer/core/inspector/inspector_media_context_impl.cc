// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_media_context_impl.h"

#include <unordered_set>
#include <utility>

#include "base/unguessable_token.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

const char MediaInspectorContextImpl::kSupplementName[] =
    "MediaInspectorContextImpl";

// static
void MediaInspectorContextImpl::ProvideToLocalFrame(LocalFrame& frame) {
  frame.ProvideSupplement(
      MakeGarbageCollected<MediaInspectorContextImpl>(frame));
}

// static
MediaInspectorContextImpl* MediaInspectorContextImpl::FromLocalFrame(
    LocalFrame* frame) {
  return Supplement<LocalFrame>::From<MediaInspectorContextImpl>(frame);
}

// static
MediaInspectorContextImpl* MediaInspectorContextImpl::FromDocument(
    const Document& document) {
  return MediaInspectorContextImpl::FromLocalFrame(document.GetFrame());
}

// static
MediaInspectorContextImpl* MediaInspectorContextImpl::FromHtmlMediaElement(
    const HTMLMediaElement& element) {
  return MediaInspectorContextImpl::FromDocument(element.GetDocument());
}

MediaInspectorContextImpl::MediaInspectorContextImpl(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {}

// Garbage collection method.
void MediaInspectorContextImpl::Trace(blink::Visitor* visitor) {
  Supplement<LocalFrame>::Trace(visitor);
  visitor->Trace(players_);
}

Vector<WebString> MediaInspectorContextImpl::GetAllPlayerIds() {
  Vector<WebString> existing_players;
  existing_players.ReserveCapacity(players_.size());
  for (const auto& player_id : players_.Keys())
    existing_players.push_back(player_id);
  return existing_players;
}

std::pair<Vector<InspectorPlayerProperty>, Vector<InspectorPlayerEvent>>
MediaInspectorContextImpl::GetPropertiesAndEvents(const WebString& player_id) {
  Vector<InspectorPlayerProperty> to_send_properties;
  Vector<InspectorPlayerEvent> to_send_events;

  const auto& player_search = players_.find(player_id);
  if (player_search != players_.end()) {
    to_send_properties.ReserveCapacity(player_search->value->properties.size());
    for (const auto& prop : player_search->value->properties.Values())
      to_send_properties.push_back(prop);
    to_send_events.ReserveCapacity(player_search->value->events.size());
    for (const auto& event : player_search->value->events)
      to_send_events.insert(to_send_events.size(), event);
  }

  return {std::move(to_send_properties), std::move(to_send_events)};
}

WebString MediaInspectorContextImpl::CreatePlayer() {
  String next_player_id =
      String::FromUTF8(base::UnguessableToken::Create().ToString());
  players_.insert(next_player_id, MakeGarbageCollected<MediaPlayer>());
  probe::PlayersCreated(GetSupplementable(), {next_player_id});
  return next_player_id;
}

// Convert public version of event to protocol version, and send it.
void MediaInspectorContextImpl::NotifyPlayerEvents(
    WebString playerId,
    InspectorPlayerEvents events) {
  const auto& player_search = players_.find(playerId);
  if (player_search == players_.end())
    DCHECK(false);
  Vector<InspectorPlayerEvent> to_send;
  to_send.ReserveCapacity(events.size());
  for (const auto& event : events) {
    player_search->value->events.emplace_back(event);
    to_send.push_back(event);
  }
  probe::PlayerEventsAdded(GetSupplementable(), playerId, to_send);
}

void MediaInspectorContextImpl::SetPlayerProperties(
    WebString playerId,
    InspectorPlayerProperties props) {
  const auto& player_search = players_.find(playerId);
  if (player_search == players_.end())
    DCHECK(false);
  Vector<InspectorPlayerProperty> to_send;
  to_send.ReserveCapacity(props.size());
  for (const auto& property : props) {
    to_send.push_back(property);
    player_search->value->properties.insert(property.name, property);
  }
  probe::PlayerPropertiesChanged(GetSupplementable(), playerId, to_send);
}

}  // namespace blink
