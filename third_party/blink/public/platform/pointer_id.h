// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_POINTER_ID_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_POINTER_ID_H_

#include <cstdint>

namespace blink {

// TODO(tkent): Rename this to WebPointerID to follow the naming convention
// in blink/public/.
using PointerId = std::int32_t;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_POINTER_ID_H_
