// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_ID_ASSIGNMENT_H_
#define PPAPI_SHARED_IMPL_ID_ASSIGNMENT_H_

#include <stdint.h>

#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

enum PPIdType {
  PP_ID_TYPE_MODULE,
  PP_ID_TYPE_INSTANCE,
  PP_ID_TYPE_RESOURCE,
  PP_ID_TYPE_VAR,

  // Not a real type, must be last.
  PP_ID_TYPE_COUNT
};

PPAPI_SHARED_EXPORT extern const unsigned int kPPIdTypeBits;

extern const int32_t kMaxPPId;

// The least significant bits are the type, the rest are the value.
template <typename T>
inline T MakeTypedId(T value, PPIdType type) {
  return (value << kPPIdTypeBits) | static_cast<T>(type);
}

template <typename T>
inline bool CheckIdType(T id, PPIdType type) {
  // Say a resource of 0 is always valid, since that means "no resource."
  // You shouldn't be passing 0 var, instance, or module IDs around so those
  // are still invalid.
  if (type == PP_ID_TYPE_RESOURCE && !id)
    return true;
  const T mask = (static_cast<T>(1) << kPPIdTypeBits) - 1;
  return (id & mask) == type;
}

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_ID_ASSIGNMENT_H_
