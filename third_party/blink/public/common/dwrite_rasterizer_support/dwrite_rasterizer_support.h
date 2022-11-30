// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_DWRITE_RASTERIZER_SUPPORT_DWRITE_RASTERIZER_SUPPORT_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_DWRITE_RASTERIZER_SUPPORT_DWRITE_RASTERIZER_SUPPORT_H_

#include "third_party/blink/public/common/common_export.h"

namespace blink {

class BLINK_COMMON_EXPORT DWriteRasterizerSupport {
 public:
  static bool IsDWriteFactory2Available();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_DWRITE_RASTERIZER_SUPPORT_DWRITE_RASTERIZER_SUPPORT_H_
