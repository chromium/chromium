// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_FONTATIONS_FEATURE_H_
#define SKIA_FONTATIONS_FEATURE_H_

#include "base/feature_list.h"
#include "third_party/skia/include/private/base/SkAPI.h"

namespace skia {

SK_API BASE_DECLARE_FEATURE(kFontationsLinuxSystemFonts);

SK_API BASE_DECLARE_FEATURE(kFontationsAndroidSystemFonts);
}

#endif  // SKIA_FONTATIONS_FEATURE_H_
