// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_SERIALIZERS_BASE_H_
#define MEDIA_BASE_MEDIA_SERIALIZERS_BASE_H_

#include <vector>

#include "base/values.h"
#include "media/base/media_export.h"

namespace media {

namespace internal {

// Serializer specializer struct.
// All the types that base::Value's constructor can take should be passed
// by non-const values. (int, bool, std::string, char*, etc).
template <typename T>
struct MediaSerializer {
  static inline base::Value Serialize(T value) { return base::Value(value); }
};

}  // namespace internal

template <typename T>
base::Value MediaSerialize(const T& t) {
  return internal::MediaSerializer<T>::Serialize(t);
}

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_SERIALIZERS_BASE_H_
