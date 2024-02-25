// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/message_service_api.h"

namespace extensions {

namespace {
MessageServiceApi* g_message_service = nullptr;
}

MessageServiceApi* MessageServiceApi::GetMessageService() {
  return g_message_service;
}

void MessageServiceApi::SetMessageService(MessageServiceApi* message_service) {
  CHECK(!g_message_service || !message_service);
  g_message_service = message_service;
}

}  // namespace extensions
