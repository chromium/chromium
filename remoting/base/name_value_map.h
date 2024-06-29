// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper functions that allow to map enum values to strings.

#ifndef REMOTING_BASE_NAME_VALUE_MAP_H_
#define REMOTING_BASE_NAME_VALUE_MAP_H_

#include <stddef.h>

#include <string_view>

#include "base/check_op.h"

namespace remoting {

template <typename T>
struct NameMapElement {
  const T value;
  const char* const name;
};

template <typename T, size_t N>
const char* ValueToNameUnchecked(const NameMapElement<T> (&map)[N], T value) {
  for (const auto& entry : map) {
    if (entry.value == value) {
      return entry.name;
    }
  }
  return nullptr;
}

template <typename T, size_t N>
const char* ValueToName(const NameMapElement<T> (&map)[N], T value) {
  const char* result = ValueToNameUnchecked(map, value);
  DCHECK_NE(nullptr, result);
  return result;
}

template <typename T, size_t N>
bool NameToValue(const NameMapElement<T> (&map)[N],
                 std::string_view name,
                 T* result) {
  for (const auto& entry : map) {
    if (entry.name == name) {
      *result = entry.value;
      return true;
    }
  }
  return false;
}

}  // namespace remoting

#endif  // REMOTING_BASE_NAME_VALUE_MAP_H_
