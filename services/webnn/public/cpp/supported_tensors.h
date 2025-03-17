// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_SUPPORTED_TENSORS_H_
#define SERVICES_WEBNN_PUBLIC_CPP_SUPPORTED_TENSORS_H_

#include <stdint.h>

#include <algorithm>
#include <initializer_list>

#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"

namespace webnn {

struct SupportedRanks {
  uint32_t min = 0;
  uint32_t max = 0;

  static constexpr SupportedRanks Exactly(uint32_t rank) {
    return {rank, rank};
  }
  static constexpr SupportedRanks UpTo(uint32_t max) { return {0, max}; }
  static constexpr SupportedRanks NonScalarUpTo(uint32_t max) {
    return {1, max};
  }

  void IntersectWith(const SupportedRanks& other) {
    min = std::max(min, other.min);
    max = std::min(max, other.max);
    // Use {0, 0} as a fallback when two rank range intervals don't overlap.
    // This may happen when an operator is not implemented. {0, 0} technically
    // means a scalar is allowed but combined with the data types being an
    // empty set we can still represent an operator that is completely
    // unsupported.
    if (min > max) {
      min = 0;
      max = 0;
    }
  }

  friend bool operator==(const SupportedRanks& lhs, const SupportedRanks& rhs);
};

inline bool operator==(const SupportedRanks& lhs,
                       const SupportedRanks& rhs) = default;

struct SupportedTensors {
  SupportedDataTypes data_types;
  SupportedRanks ranks;

  void IntersectWith(const SupportedTensors& other) {
    data_types.RetainAll(other.data_types);
    ranks.IntersectWith(other.ranks);
  }

  bool Supports(const OperandDescriptor& operand_descriptor) const {
    uint32_t rank = operand_descriptor.Rank();
    return data_types.Has(operand_descriptor.data_type()) &&
           ranks.min <= rank && rank <= ranks.max;
  }

  bool SupportsAll(
      std::initializer_list<OperandDescriptor> operand_descriptors) const {
    return std::ranges::all_of(
        operand_descriptors,
        [this](const OperandDescriptor& operand_descriptor) {
          return Supports(operand_descriptor);
        });
  }

  friend bool operator==(const SupportedTensors& lhs,
                         const SupportedTensors& rhs);
};

inline bool operator==(const SupportedTensors& lhs,
                       const SupportedTensors& rhs) = default;

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_SUPPORTED_TENSORS_H_
