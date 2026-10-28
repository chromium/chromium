// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/message_fragment.h"

namespace mojo::internal {

void EncodeFragmentPointer(Message& message, size_t index, uint64_t* offset) {
  EncodePointer(index == kInvalidFragmentIndex
                    ? nullptr
                    : message.payload_buffer()->Get<void>(index),
                offset);
}

}  // namespace mojo::internal
