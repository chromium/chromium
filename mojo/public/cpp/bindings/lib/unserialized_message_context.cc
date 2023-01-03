// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/unserialized_message_context.h"

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/lib/message_internal.h"

namespace mojo {
namespace internal {

UnserializedMessageContext::UnserializedMessageContext(const Tag* tag,
                                                       uint32_t message_name,
                                                       uint32_t message_flags)
    : tag_(reinterpret_cast<uintptr_t>(tag)) {
  header_.interface_id = 0;
  header_.version = 1;
  header_.name = message_name;
  header_.flags = message_flags;
  header_.num_bytes = sizeof(header_);
}

UnserializedMessageContext::~UnserializedMessageContext() = default;

}  // namespace internal
}  // namespace mojo
