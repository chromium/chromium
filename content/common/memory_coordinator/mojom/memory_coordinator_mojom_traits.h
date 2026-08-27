// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MOJOM_MEMORY_COORDINATOR_MOJOM_TRAITS_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MOJOM_MEMORY_COORDINATOR_MOJOM_TRAITS_H_

#include <optional>
#include <string>

#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom-shared.h"
#include "content/public/common/memory_consumer_update.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::MemoryConsumerUpdateDataView,
                    content::MemoryConsumerUpdate> {
  static uint32_t consumer_id(const content::MemoryConsumerUpdate& input) {
    return input.consumer_id;
  }

  static const std::optional<base::MemoryLimit>& memory_limit(
      const content::MemoryConsumerUpdate& input) {
    return input.memory_limit;
  }

  static bool release_memory(const content::MemoryConsumerUpdate& input) {
    return input.release_memory;
  }

  static bool Read(content::mojom::MemoryConsumerUpdateDataView input,
                   content::MemoryConsumerUpdate* output) {
    output->consumer_id = input.consumer_id();
    output->release_memory = input.release_memory();
    return input.ReadMemoryLimit(&output->memory_limit);
  }
};

}  // namespace mojo

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MOJOM_MEMORY_COORDINATOR_MOJOM_TRAITS_H_
