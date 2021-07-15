// Copyright 2021 The Chromium Authors. All rights reserved.
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
                            base::Value& dictionary_value) {
  auto opt_message = base::JSONReader::Read(message);
  if (!opt_message.has_value()) {
    LOG(ERROR) << "Received a message that's not valid JSON.";
    return false;
  }

  auto message_value = std::move(opt_message.value());
  if (!message_value.is_dict()) {
    LOG(ERROR) << "Received a message that's not a dictionary.";
    return false;
  }

  const std::string* message_type_value =
      message_value.FindStringKey(kMessageType);
  if (message_type_value) {
    message_type = *message_type_value;
  }

  dictionary_value = std::move(message_value);

  return true;
}

base::Value CreateNativeMessageResponse(const base::Value& request) {
  const std::string* type = request.FindStringKey(kMessageType);
  if (!type) {
    LOG(ERROR) << "'" << kMessageType << "' not found in request.";
    return base::Value();
  }

  base::Value response(base::Value::Type::DICTIONARY);
  response.SetStringKey(kMessageType, *type + "Response");

  // If the client supplies an ID, it will expect it in the response. This
  // might be a string or a number, so cope with both.
  const base::Value* id = request.FindKey(kMessageId);
  if (id) {
    response.SetKey(kMessageId, id->Clone());
  }
  return response;
}

void ProcessNativeMessageHelloResponse(base::Value& response,
                                       base::Value supported_features) {
  response.SetStringKey(kHostVersion, STRINGIZE(VERSION));
  if (!supported_features.is_none()) {
    response.SetKey(kSupportedFeatures, std::move(supported_features));
  }
}

}  // namespace remoting
