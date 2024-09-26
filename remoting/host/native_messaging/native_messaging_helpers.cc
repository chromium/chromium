// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/native_messaging/native_messaging_helpers.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/stringize_macros.h"
#include "remoting/host/native_messaging/native_messaging_constants.h"

namespace remoting {

bool ParseNativeMessageJson(const std::string& message,
                            std::string& message_type,
                            base::Value::Dict& parsed_message) {
  auto opt_message = base::JSONReader::Read(message);
  if (!opt_message.has_value()) {
    LOG(ERROR) << "Received a message that's not valid JSON.";
    return false;
  }

  auto message_value = std::move(*opt_message);
  if (!message_value.is_dict()) {
    LOG(ERROR) << "Received a message that's not a dictionary.";
    return false;
  }

  const std::string* message_type_value =
      message_value.GetDict().FindString(kMessageType);
  if (message_type_value) {
    message_type = *message_type_value;
  }

  parsed_message = std::move(message_value).TakeDict();

  return true;
}

std::optional<base::Value::Dict> CreateNativeMessageResponse(
    const base::Value::Dict& request) {
  const std::string* type = request.FindString(kMessageType);
  if (!type) {
    LOG(ERROR) << "'" << kMessageType << "' not found in request.";
    return std::nullopt;
  }

  base::Value::Dict response;
  response.Set(kMessageType, *type + "Response");

  // If the client supplies an ID, it will expect it in the response. This
  // might be a string or a number, so cope with both.
  const base::Value* id = request.Find(kMessageId);
  if (id) {
    response.Set(kMessageId, id->Clone());
  }
  return response;
}

void ProcessNativeMessageHelloResponse(base::Value::Dict& response,
                                       base::Value::List supported_features) {
  response.Set(kHostVersion, STRINGIZE(VERSION));
  if (!supported_features.empty()) {
    response.Set(kSupportedFeatures, std::move(supported_features));
  }
}

}  // namespace remoting
