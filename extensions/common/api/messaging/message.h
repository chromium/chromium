// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_MESSAGING_MESSAGE_H_
#define EXTENSIONS_COMMON_API_MESSAGING_MESSAGE_H_

namespace extensions {

// A message consists of both the data itself as well as a user gesture state.
struct Message {
  std::string data;
  bool user_gesture = false;
  bool from_privileged_context = false;

  Message() = default;
  Message(const std::string& data,
          bool user_gesture,
          bool from_privileged_context = false)
      : data(data),
        user_gesture(user_gesture),
        from_privileged_context(from_privileged_context) {}

  bool operator==(const Message& other) const {
    // Skipping the equality check for |from_privileged_context| here
    // because this field is used only for histograms.
    return data == other.data && user_gesture == other.user_gesture;
  }
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_MESSAGING_MESSAGE_H_
