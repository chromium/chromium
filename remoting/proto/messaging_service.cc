// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/proto/messaging_service.h"

namespace remoting::internal {

ReceiveClientMessagesResponseStruct::ReceiveClientMessagesResponseStruct() {}
ReceiveClientMessagesResponseStruct::~ReceiveClientMessagesResponseStruct() {}

SimpleMessageStruct::SimpleMessageStruct() = default;
SimpleMessageStruct::SimpleMessageStruct(const SimpleMessageStruct&) = default;
SimpleMessageStruct& SimpleMessageStruct::operator=(
    const SimpleMessageStruct&) = default;
SimpleMessageStruct::~SimpleMessageStruct() = default;

}  // namespace remoting::internal
