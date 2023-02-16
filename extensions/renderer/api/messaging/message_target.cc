// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/messaging/message_target.h"

namespace extensions {

MessageTarget MessageTarget::ForTab(int tab_id, int frame_id) {
  MessageTarget target(TAB);
  target.tab_id = tab_id;
  target.frame_id = frame_id;
  return target;
}

MessageTarget MessageTarget::ForTab(int tab_id,
                                    int frame_id,
                                    const std::string& document_id) {
  MessageTarget target(TAB);
  target.tab_id = tab_id;
  target.frame_id = frame_id;
  if (!document_id.empty())
    target.document_id = document_id;
  return target;
}

MessageTarget MessageTarget::ForExtension(const ExtensionId& extension_id) {
  MessageTarget target(EXTENSION);
  target.extension_id = extension_id;
  return target;
}

MessageTarget MessageTarget::ForNativeApp(const std::string& native_app) {
  MessageTarget target(NATIVE_APP);
  target.native_application_name = native_app;
  return target;
}

MessageTarget::MessageTarget(MessageTarget&& other) = default;
MessageTarget::MessageTarget(const MessageTarget& other) = default;
MessageTarget::~MessageTarget() = default;

bool MessageTarget::operator==(const MessageTarget& other) const {
  return type == other.type && extension_id == other.extension_id &&
         native_application_name == other.native_application_name &&
         tab_id == other.tab_id && frame_id == other.frame_id;
}

MessageTarget::MessageTarget(Type type) : type(type) {}

}  // namespace extensions
