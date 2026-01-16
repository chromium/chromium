// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the definition of ValueValidator for the uses in *_cmd_validation.h

#ifndef GPU_COMMAND_BUFFER_SERVICE_VALUE_VALIDATOR_H_
#define GPU_COMMAND_BUFFER_SERVICE_VALUE_VALIDATOR_H_

#include <algorithm>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"

namespace gpu {

// ValueValidator returns true if a value is valid.
template <typename T>
class ValueValidator {
 public:
  ValueValidator() = default;

  ValueValidator(base::span<const T> valid_values) { AddValues(valid_values); }

  void AddValue(const T value) {
    if (!IsValid(value)) {
      valid_values_.push_back(value);
    }
  }

  void AddValues(base::span<const T> valid_values) {
    for (const T& value : valid_values) {
      AddValue(value);
    }
  }

  void RemoveValues(base::span<const T> invalid_values) {
    for (const auto& value : invalid_values) {
      auto iter = std::ranges::find(valid_values_, value);
      if (iter != valid_values_.end()) {
        valid_values_.erase(iter);
        DCHECK(!IsValid(value));
      }
    }
  }

  bool IsValid(const T value) const {
    return base::Contains(valid_values_, value);
  }

  const std::vector<T>& GetValues() const { return valid_values_; }

 private:
  std::vector<T> valid_values_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_VALUE_VALIDATOR_H_
