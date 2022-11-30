// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_DISCONNECT_REASON_H_
#define MOJO_PUBLIC_CPP_BINDINGS_DISCONNECT_REASON_H_

#include <stdint.h>

#include <string>

namespace mojo {

struct DisconnectReason {
 public:
  DisconnectReason(uint32_t in_custom_reason, const std::string& in_description)
      : custom_reason(in_custom_reason), description(in_description) {}

  uint32_t custom_reason;
  std::string description;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_DISCONNECT_REASON_H_
