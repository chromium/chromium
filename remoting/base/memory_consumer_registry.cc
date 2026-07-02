// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/memory_consumer_registry.h"

namespace remoting {

MemoryConsumerRegistry::MemoryConsumerRegistry() = default;

MemoryConsumerRegistry::~MemoryConsumerRegistry() {
  NotifyDestruction();
}

void MemoryConsumerRegistry::OnMemoryConsumerAdded(
    uint32_t consumer_id,
    std::string_view consumer_name,
    std::optional<base::MemoryConsumerTraits> traits,
    base::MemoryConsumer* consumer) {}

void MemoryConsumerRegistry::OnMemoryConsumerRemoved(
    uint32_t consumer_id,
    base::MemoryConsumer* consumer) {}

}  // namespace remoting
