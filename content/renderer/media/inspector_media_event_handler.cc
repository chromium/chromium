// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/inspector_media_event_handler.h"

#include <string>
#include <utility>

#include "base/json/json_writer.h"

namespace content {

namespace {

blink::WebString ToString(const base::Value& value) {
  if (value.is_string()) {
    return blink::WebString::FromUTF8(value.GetString());
  }
  std::string output_str;
  base::JSONWriter::Write(value, &output_str);
  return blink::WebString::FromUTF8(output_str);
}

}  // namespace

InspectorMediaEventHandler::InspectorMediaEventHandler(
    blink::MediaInspectorContext* inspector_context)
    : inspector_context_(inspector_context),
      player_id_(inspector_context_->CreatePlayer()) {}

// TODO(tmathmeyer) It would be wonderful if the definition for MediaLogEvent
// and InspectorPlayerEvent / InspectorPlayerProperty could be unified so that
// this method is no longer needed. Refactor MediaLogEvent at some point.
void InspectorMediaEventHandler::SendQueuedMediaEvents(
    std::vector<media::MediaLogEvent> events_to_send) {
  // If the video player is gone, the whole frame
  if (video_player_destroyed_)
    return;

  blink::InspectorPlayerEvents events;
  blink::InspectorPlayerProperties properties;

  for (media::MediaLogEvent event : events_to_send) {
    if (event.type == media::MediaLogEvent::PROPERTY_CHANGE) {
      for (auto&& itr : event.params.DictItems()) {
        blink::InspectorPlayerProperty prop = {
            blink::WebString::FromUTF8(itr.first), ToString(itr.second)};
        properties.emplace_back(prop);
      }
    } else {
      blink::InspectorPlayerEvent::InspectorPlayerEventType event_type =
          blink::InspectorPlayerEvent::SYSTEM_EVENT;

      if (event.type == media::MediaLogEvent::MEDIA_ERROR_LOG_ENTRY ||
          event.type == media::MediaLogEvent::MEDIA_WARNING_LOG_ENTRY ||
          event.type == media::MediaLogEvent::MEDIA_INFO_LOG_ENTRY ||
          event.type == media::MediaLogEvent::MEDIA_DEBUG_LOG_ENTRY) {
        event_type = blink::InspectorPlayerEvent::MESSAGE_EVENT;
      }
      if (event.params.size() == 0) {
        blink::InspectorPlayerEvent ev = {
            blink::InspectorPlayerEvent::PLAYBACK_EVENT, event.time,
            blink::WebString::FromUTF8("Event"),
            blink::WebString::FromUTF8(
                media::MediaLog::EventTypeToString(event.type))};
        events.emplace_back(ev);
      }
      for (auto&& itr : event.params.DictItems()) {
        blink::InspectorPlayerEvent ev = {event_type, event.time,
                                          blink::WebString::FromUTF8(itr.first),
                                          ToString(itr.second)};
        events.emplace_back(ev);
      }
    }
  }
  if (!events.empty())
    inspector_context_->NotifyPlayerEvents(player_id_, events);

  if (!properties.empty())
    inspector_context_->SetPlayerProperties(player_id_, properties);
}

void InspectorMediaEventHandler::OnWebMediaPlayerDestroyed() {
  video_player_destroyed_ = true;
}

}  // namespace content
