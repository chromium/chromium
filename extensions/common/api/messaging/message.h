// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_MESSAGING_MESSAGE_H_
#define EXTENSIONS_COMMON_API_MESSAGING_MESSAGE_H_

#include <string>

#include "extensions/common/mojom/message_port.mojom-shared.h"

namespace extensions {

// A message consists of both the data itself as well as a user gesture state.
class Message {
 public:
  Message();
  Message(const std::string& data,
          mojom::SerializationFormat format,
          bool user_gesture,
          bool from_privileged_context = false);
  Message(const Message& other);
  Message(Message&& other);
  ~Message();

  Message& operator=(const Message& other);
  Message& operator=(Message&& other);

  bool operator==(const Message& other) const;

  const std::string& data() const { return data_; }
  mojom::SerializationFormat format() const { return format_; }
  bool user_gesture() const { return user_gesture_; }
  bool from_privileged_context() const { return from_privileged_context_; }

 private:
  std::string data_;
  mojom::SerializationFormat format_ = mojom::SerializationFormat::kJson;
  bool user_gesture_ = false;
  bool from_privileged_context_ = false;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_MESSAGING_MESSAGE_H_
