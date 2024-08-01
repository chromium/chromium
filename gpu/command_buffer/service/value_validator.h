// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Contains the definition of ValueValidator for the uses in *_cmd_validation.h

#ifndef GPU_COMMAND_BUFFER_SERVICE_VALUE_VALIDATOR_H_
#define GPU_COMMAND_BUFFER_SERVICE_VALUE_VALIDATOR_H_

#include <vector>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"

namespace gpu {

// ValueValidator returns true if a value is valid.
template <typename T>
class ValueValidator {
 public:
  ValueValidator() = default;

  ValueValidator(const T* valid_values, int num_values) {
    AddValues(valid_values, num_values);
  }

  void AddValue(const T value) {
    if (!IsValid(value)) {
      valid_values_.push_back(value);
    }
  }

  void AddValues(const T* valid_values, int num_values) {
    for (int ii = 0; ii < num_values; ++ii) {
      AddValue(valid_values[ii]);
    }
  }

  void RemoveValues(const T* invalid_values, int num_values) {
    for (int ii = 0; ii < num_values; ++ii) {
      auto iter = base::ranges::find(valid_values_, invalid_values[ii]);
      if (iter != valid_values_.end()) {
        valid_values_.erase(iter);
        DCHECK(!IsValid(invalid_values[ii]));
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
