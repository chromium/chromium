// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/transferable_message.h"

#include "third_party/blink/public/mojom/array_buffer/array_buffer_contents.mojom.h"

namespace blink {

TransferableMessage::TransferableMessage() = default;
TransferableMessage::TransferableMessage(TransferableMessage&&) = default;
TransferableMessage& TransferableMessage::operator=(TransferableMessage&&) =
    default;
TransferableMessage::~TransferableMessage() = default;

}  // namespace blink
