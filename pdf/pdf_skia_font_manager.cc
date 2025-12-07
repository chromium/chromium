// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkFontMgr_Fontations.h"

sk_sp<SkFontMgr> pdfium_skia_custom_font_manager() {
  return SkFontMgr_New_Fontations_Empty();
}
