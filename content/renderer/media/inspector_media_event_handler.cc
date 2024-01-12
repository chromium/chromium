// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/inspector_media_event_handler.h"

#include <string>
#include <utility>

#include "base/json/json_writer.h"

namespace content {

namespace {

std::optional<blink::InspectorPlayerError> ErrorFromParams(
    const base::Value::Dict& param) {
  std::optional<int> code = param.FindInt(media::StatusConstants::kCodeKey);
  const std::string* group =
      param.FindString(media::StatusConstants::kGroupKey);
  const std::string* message =
      param.FindString(media::StatusConstants::kMsgKey);

  // message might be empty or not present, but group and code are required.
  CHECK(code.has_value() && group);

  blink::InspectorPlayerErrors caused_by;
  if (const auto* c = param.FindDict(media::StatusConstants::kCauseKey)) {
    auto parsed_cause = ErrorFromParams(*c);
    if (parsed_cause.has_value())
      caused_by.push_back(*parsed_cause);
  }

  blink::WebVector<blink::InspectorPlayerError::SourceLocation> stack_vec;
  if (const auto* vec = param.FindList(media::StatusConstants::kStackKey)) {
    for (const auto& loc : *vec) {
      const auto& loc_dict = loc.GetDict();
      const std::string* file =
          loc_dict.FindString(media::StatusConstants::kFileKey);
      std::optional<int> line =
          loc_dict.FindInt(media::StatusConstants::kLineKey);
      if (!file || !line.has_value())
        continue;
      blink::InspectorPlayerError::SourceLocation entry = {
          blink::WebString::FromUTF8(*file), *line};
      stack_vec.push_back(std::move(entry));
    }
  }

  blink::WebVector<blink::InspectorPlayerError::Data> data_vec;
  if (auto* data = param.FindDict(media::StatusConstants::kDataKey)) {
    for (const auto pair : *data) {
      std::string json;
      base::JSONWriter::Write(pair.second, &json);
      blink::InspectorPlayerError::Data entry = {
          blink::WebString::FromUTF8(pair.first),
          blink::WebString::FromUTF8(json)};
      data_vec.push_back(std::move(entry));
    }
  }

  blink::InspectorPlayerError result = {
      blink::WebString::FromUTF8(*group),
      *code,
      blink::WebString::FromUTF8(message ? *message : ""),
      std::move(stack_vec),
      std::move(caused_by),
      std::move(data_vec)};

  return std::move(result);
}

blink::WebString ToString(const base::Value& value) {
  if (value.is_string()) {
    return blink::WebString::FromUTF8(value.GetString());
  }
  std::string output_str;
  base::JSONWriter::Write(value, &output_str);
  return blink::WebString::FromUTF8(output_str);
}

blink::WebString ToString(const base::Value::Dict& value) {
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
  CHECK_EQ(level, "debug");
  return blink::InspectorPlayerMessage::Level::kDebug;
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
        for (auto&& itr : event.params) {
          blink::InspectorPlayerMessage msg = {
              LevelFromString(itr.first),
              blink::WebString::FromUTF8(itr.second.GetString())};
          messages.emplace_back(std::move(msg));
        }
        break;
      }
      case media::MediaLogRecord::Type::kMediaPropertyChange: {
        for (auto&& itr : event.params) {
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
        std::optional<blink::InspectorPlayerError> error =
            ErrorFromParams(event.params);
        if (error.has_value())
          errors.emplace_back(std::move(*error));
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
  inspector_context_->DestroyPlayer(player_id_);
}

}  // namespace content
