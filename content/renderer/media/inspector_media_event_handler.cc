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

// TODO(tmathmeyer) stop using a string here eventually. This means rewriting
// the MediaLogRecord mojom interface.
blink::InspectorPlayerMessage::Level LevelFromString(const std::string& level) {
  if (level == "error")
    return blink::InspectorPlayerMessage::Level::kError;
  if (level == "warning")
    return blink::InspectorPlayerMessage::Level::kWarning;
  if (level == "info")
    return blink::InspectorPlayerMessage::Level::kInfo;
  if (level == "debug")
    return blink::InspectorPlayerMessage::Level::kDebug;
  NOTREACHED();
  return blink::InspectorPlayerMessage::Level::kError;
}

}  // namespace

InspectorMediaEventHandler::InspectorMediaEventHandler(
    blink::MediaInspectorContext* inspector_context)
    : inspector_context_(inspector_context),
      player_id_(inspector_context_->CreatePlayer()) {}

// TODO(tmathmeyer) It would be wonderful if the definition for MediaLogRecord
// and InspectorPlayerEvent / InspectorPlayerProperty could be unified so that
// this method is no longer needed. Refactor MediaLogRecord at some point.
void InspectorMediaEventHandler::SendQueuedMediaEvents(
    std::vector<media::MediaLogRecord> events_to_send) {
  // If the video player is gone, the whole frame
  if (video_player_destroyed_)
    return;

  blink::InspectorPlayerProperties properties;
  blink::InspectorPlayerMessages messages;
  blink::InspectorPlayerEvents events;
  blink::InspectorPlayerErrors errors;

  for (media::MediaLogRecord event : events_to_send) {
    switch (event.type) {
      case media::MediaLogRecord::Type::kMessage: {
        for (auto&& itr : event.params.DictItems()) {
          blink::InspectorPlayerMessage msg = {
              LevelFromString(itr.first),
              blink::WebString::FromUTF8(itr.second.GetString())};
          messages.emplace_back(std::move(msg));
        }
        break;
      }
      case media::MediaLogRecord::Type::kMediaPropertyChange: {
        for (auto&& itr : event.params.DictItems()) {
          blink::InspectorPlayerProperty prop = {
              blink::WebString::FromUTF8(itr.first), ToString(itr.second)};
          properties.emplace_back(std::move(prop));
        }
        break;
      }
      case media::MediaLogRecord::Type::kMediaEventTriggered: {
        blink::InspectorPlayerEvent ev = {event.time, ToString(event.params)};
        events.emplace_back(std::move(ev));
        break;
      }
      case media::MediaLogRecord::Type::kMediaStatus: {
        base::Value* code = event.params.FindKey(media::MediaLog::kStatusText);
        DCHECK_NE(code, nullptr);
        blink::InspectorPlayerError error = {
            blink::InspectorPlayerError::Type::kPipelineError, ToString(*code)};
        errors.emplace_back(std::move(error));
        break;
      }
    }
  }

  if (!events.empty())
    inspector_context_->NotifyPlayerEvents(player_id_, std::move(events));

  if (!properties.empty())
    inspector_context_->SetPlayerProperties(player_id_, std::move(properties));

  if (!messages.empty())
    inspector_context_->NotifyPlayerMessages(player_id_, std::move(messages));

  if (!errors.empty())
    inspector_context_->NotifyPlayerErrors(player_id_, std::move(errors));
}

void InspectorMediaEventHandler::OnWebMediaPlayerDestroyed() {
  video_player_destroyed_ = true;
}

}  // namespace content
