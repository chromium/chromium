// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_MESSAGE_UTIL_H_
#define PDF_MESSAGE_UTIL_H_

#include "base/values.h"

namespace chrome_pdf {

// Prepares messages from the plugin that reply to messages from the embedder.
// If the "type" value of `message` is "foo", then the reply "type" will be
// "fooReply". The `message` from the embedder must have a "messageId" value
// that will be copied to the reply message.
base::Value::Dict PrepareReplyMessage(const base::Value::Dict& message);

}  // namespace chrome_pdf

#endif  // PDF_MESSAGE_UTIL_H_
