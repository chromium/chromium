// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Serialization and deserialization code for some //base types that are used in
// //net. This can't be put in //base because //base can't depend on //net.

#ifndef NET_BASE_PICKLE_BASE_TYPES_H_
#define NET_BASE_PICKLE_BASE_TYPES_H_

#include <stdint.h>

#include <optional>

#include "base/time/time.h"
#include "net/base/pickle_traits.h"

namespace net {

template <>
struct PickleTraits<base::Time> {
  static void Serialize(base::Pickle& pickle, base::Time time) {
    // For compatibility with existing serialization code in //net, use the
    // deprecated `ToInternalValue()` method.
    pickle.WriteInt64(time.ToInternalValue());
  }

  static std::optional<base::Time> Deserialize(base::PickleIterator& iter) {
    int64_t time_as_int64;
    if (!iter.ReadInt64(&time_as_int64)) {
      return std::nullopt;
    }
    return base::Time::FromInternalValue(time_as_int64);
  }

  static size_t PickleSize(base::Time time) {
    return EstimatePickleSize(int64_t{0});
  }
};

}  // namespace net

#endif  // NET_BASE_PICKLE_BASE_TYPES_H_
