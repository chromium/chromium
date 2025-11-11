// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_MEMORY_COORDINATOR_MEMORY_CONSUMER_REGISTRATION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_MEMORY_COORDINATOR_MEMORY_CONSUMER_REGISTRATION_H_

#include <optional>
#include <string_view>

#include "base/memory_coordinator/async_memory_consumer_registration.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// A version of base::MemoryConsumerRegistration that is compatible with
// garbage-collected classes, by forcing the user to unregister the listener
// before the destructor is called. The registration is always done
// asynchronously, to support --single-process mode.
// TODO(pmonette): Investigate making this sync whenever possible.
class PLATFORM_EXPORT MemoryConsumerRegistration {
 public:
  using CheckUnregister = base::MemoryConsumerRegistration::CheckUnregister;
  using CheckRegistryExists =
      base::MemoryConsumerRegistration::CheckRegistryExists;

  MemoryConsumerRegistration(
      std::string_view consumer_id,
      base::MemoryConsumerTraits traits,
      base::MemoryConsumer* consumer,
      CheckUnregister check_unregister = CheckUnregister::kEnabled,
      CheckRegistryExists check_registry_exists =
          CheckRegistryExists::kEnabled);
  ~MemoryConsumerRegistration();

  // Cancels the registration. This must be invoked manually whenever the
  // registration is no longer needed.
  void Dispose();

 private:
  std::optional<base::AsyncMemoryConsumerRegistration> registration_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_MEMORY_COORDINATOR_MEMORY_CONSUMER_REGISTRATION_H_
