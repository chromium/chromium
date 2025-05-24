// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/message_util.h"

#include <string>

#include "base/values.h"

namespace chrome_pdf {

base::Value::Dict PrepareReplyMessage(const base::Value::Dict& message) {
  const std::string* original_type = message.FindString("type");
  CHECK(original_type);

  const std::string* message_id = message.FindString("messageId");
  CHECK(message_id);

  base::Value::Dict reply;
  reply.Set("type", *original_type + "Reply");
  reply.Set("messageId", *message_id);
  return reply;
}

}  // namespace chrome_pdf
