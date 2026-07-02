// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_MEMORY_CONSUMER_REGISTRY_H_
#define REMOTING_BASE_MEMORY_CONSUMER_REGISTRY_H_

#include <cstdint>
#include <optional>
#include <string_view>

#include "base/memory_coordinator/memory_consumer_registry.h"

namespace remoting {

// An implementation of MemoryConsumerRegistry for Chrome Remote Desktop
// standalone host and client processes. Since network classes (like
// SSLClientSessionCache) register as memory consumers, they expect a global
// memory consumer registry to exist. Without a registered instance, the
// MemoryConsumerRegistration constructor fails a CHECK_IS_TEST() assertion
// outside of unit tests and crashes the process.
class MemoryConsumerRegistry : public base::MemoryConsumerRegistry {
 public:
  MemoryConsumerRegistry();

  MemoryConsumerRegistry(const MemoryConsumerRegistry&) = delete;
  MemoryConsumerRegistry& operator=(const MemoryConsumerRegistry&) = delete;

  ~MemoryConsumerRegistry() override;

 protected:
  // base::MemoryConsumerRegistry:
  void OnMemoryConsumerAdded(uint32_t consumer_id,
                             std::string_view consumer_name,
                             std::optional<base::MemoryConsumerTraits> traits,
                             base::MemoryConsumer* consumer) override;
  void OnMemoryConsumerRemoved(uint32_t consumer_id,
                               base::MemoryConsumer* consumer) override;
};

}  // namespace remoting

#endif  // REMOTING_BASE_MEMORY_CONSUMER_REGISTRY_H_
