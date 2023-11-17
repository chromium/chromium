// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/font_utils.h"

#include "base/check.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace skia {

sk_sp<SkFontMgr> DefaultFontMgr() {
  // TODO(b/305780908) Replace this with a singleton that depends on which
  // platform we are on and which SkFontMgr was compiled in (see
  // //src/skia/BUILD.gn)
  return SkFontMgr::RefDefault();
}

sk_sp<SkTypeface> MakeTypefaceFromName(const char* name, SkFontStyle style) {
  sk_sp<SkFontMgr> fm = DefaultFontMgr();
  CHECK(fm);
  sk_sp<SkTypeface> face = fm->legacyMakeTypeface(name, style);
  return face;
}

sk_sp<SkTypeface> DefaultTypeface() {
  sk_sp<SkTypeface> face = MakeTypefaceFromName(nullptr, SkFontStyle());
  if (face) {
    return face;
  }
  // Due to how SkTypeface::MakeDefault() used to work, many callers of this
  // depend on the returned SkTypeface being non-null. An empty Typeface is
  // non-null, but has no glyphs.
  face = SkTypeface::MakeEmpty();
  CHECK(face);
  return face;
}

SkFont DefaultFont() {
  return SkFont(DefaultTypeface());
}

}  // namespace skia
