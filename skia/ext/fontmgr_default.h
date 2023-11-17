// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_FONTMGR_DEFAULT_H_
#define SKIA_EXT_FONTMGR_DEFAULT_H_

#include "third_party/skia/include/core/SkTypes.h"

class SkFontMgr;
template <typename T>
class sk_sp;

namespace skia {

// Allows to override the default SkFontMgr instance (returned from
// skia::DefaultFontMgr()). Must be called before skia::DefaultFontMgr() is
// called for the first time in the process.
SK_API void OverrideDefaultSkFontMgr(sk_sp<SkFontMgr> fontmgr);

// Create default SkFontMgr implementation for the current platform.
SK_API sk_sp<SkFontMgr> CreateDefaultSkFontMgr();

}  // namespace skia

#endif  // SKIA_EXT_FONTMGR_DEFAULT_H_
