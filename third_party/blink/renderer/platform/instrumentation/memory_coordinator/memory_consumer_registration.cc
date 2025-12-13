// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/memory_coordinator/memory_consumer_registration.h"

namespace blink {

MemoryConsumerRegistration::MemoryConsumerRegistration(
    std::string_view consumer_id,
    base::MemoryConsumerTraits traits,
    base::MemoryConsumer* consumer,
    CheckUnregister check_unregister,
    CheckRegistryExists check_registry_exists)
    : registration_(std::in_place,
                    consumer_id,
                    traits,
                    consumer,
                    check_unregister,
                    check_registry_exists) {}

MemoryConsumerRegistration::~MemoryConsumerRegistration() {
  CHECK(!registration_);
}

void MemoryConsumerRegistration::Dispose() {
  registration_.reset();
}

}  // namespace blink
