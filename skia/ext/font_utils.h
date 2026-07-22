// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_FONT_UTILS_H_
#define SKIA_EXT_FONT_UTILS_H_

#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypes.h"

class SkData;
class SkFont;
class SkFontMgr;
class SkTypeface;

namespace skia {

// OTS-transcodes/sanitizes the embedded font data stream.
// Returns the pristine, normalized SkData on success, or nullptr on rejection.
SK_API sk_sp<SkData> SanitizeTypefaceStream(sk_sp<const SkData> data);

// Returns the platform specific SkFontMgr, which is a singleton.
SK_API sk_sp<SkFontMgr> DefaultFontMgr();

// Allows to override the default SkFontMgr instance (returned from
// skia::DefaultFontMgr()). Must be called before skia::DefaultFontMgr() is
// called for the first time in the process.
SK_API void OverrideDefaultSkFontMgr(sk_sp<SkFontMgr> fontmgr);

// Returns a default SkTypeface returned by a platform-specific SkFontMgr.
SK_API sk_sp<SkTypeface> DefaultTypeface();

// Returns a Typeface matching the given criteria as returned the
// platform-specific SkFontMgr that was compiled in. This Typeface may be
// different on different platforms.
SK_API sk_sp<SkTypeface> MakeTypefaceFromName(const char* name,
                                              SkFontStyle style);

// Returns a font using DefaultTypeface()
SK_API SkFont DefaultFont();

SK_API void InitializeFontRendering();

}  // namespace skia

#endif  // SKIA_EXT_FONT_UTILS_H_
