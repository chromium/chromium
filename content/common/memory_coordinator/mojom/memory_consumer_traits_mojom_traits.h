// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MOJOM_MEMORY_CONSUMER_TRAITS_MOJOM_TRAITS_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MOJOM_MEMORY_CONSUMER_TRAITS_MOJOM_TRAITS_H_

#include "base/memory_coordinator/traits.h"
#include "base/types/cxx23_to_underlying.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::MemoryConsumerTraitsDataView,
                    base::MemoryConsumerTraits> {
  template <class EnumType>
  static bool ConvertToEnum(uint8_t input, EnumType* out) {
    if (input < 0 || input > base::to_underlying(EnumType::kMaxValue)) {
      return false;
    }

    *out = static_cast<EnumType>(input);
    return true;
  }

  static uint8_t supports_memory_limit(
      const base::MemoryConsumerTraits& input) {
    return base::to_underlying(input.supports_memory_limit);
  }
  static uint8_t in_process(const base::MemoryConsumerTraits& input) {
    return base::to_underlying(input.in_process);
  }
  static uint8_t estimated_memory_usage(
      const base::MemoryConsumerTraits& input) {
    return base::to_underlying(input.estimated_memory_usage);
  }
  static uint8_t release_memory_cost(const base::MemoryConsumerTraits& input) {
    return base::to_underlying(input.release_memory_cost);
  }
  static uint8_t recreate_memory_cost(const base::MemoryConsumerTraits& input) {
    return base::to_underlying(input.recreate_memory_cost);
  }
  static uint8_t information_retention(
      const base::MemoryConsumerTraits& input) {
    return base::to_underlying(input.information_retention);
  }
  static uint8_t memory_release_behavior(
      const base::MemoryConsumerTraits& input) {
    return base::to_underlying(input.memory_release_behavior);
  }
  static uint8_t execution_type(const base::MemoryConsumerTraits& input) {
    return base::to_underlying(input.execution_type);
  }
  static uint8_t release_gc_references(
      const base::MemoryConsumerTraits& input) {
    return base::to_underlying(input.release_gc_references);
  }
  static uint8_t garbage_collects_v8_heap(
      const base::MemoryConsumerTraits& input) {
    return base::to_underlying(input.garbage_collects_v8_heap);
  }

  static bool Read(content::mojom::MemoryConsumerTraitsDataView input,
                   base::MemoryConsumerTraits* output) {
    return ConvertToEnum(input.supports_memory_limit(),
                         &output->supports_memory_limit) &&
           ConvertToEnum(input.in_process(), &output->in_process) &&
           ConvertToEnum(input.estimated_memory_usage(),
                         &output->estimated_memory_usage) &&
           ConvertToEnum(input.release_memory_cost(),
                         &output->release_memory_cost) &&
           ConvertToEnum(input.recreate_memory_cost(),
                         &output->recreate_memory_cost) &&
           ConvertToEnum(input.information_retention(),
                         &output->information_retention) &&
           ConvertToEnum(input.memory_release_behavior(),
                         &output->memory_release_behavior) &&
           ConvertToEnum(input.execution_type(), &output->execution_type) &&
           ConvertToEnum(input.release_gc_references(),
                         &output->release_gc_references) &&
           ConvertToEnum(input.garbage_collects_v8_heap(),
                         &output->garbage_collects_v8_heap);
  }
};

}  // namespace mojo

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MOJOM_MEMORY_CONSUMER_TRAITS_MOJOM_TRAITS_H_
