// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/test_fonts_fuchsia.h"

#include <fuchsia/fonts/cpp/fidl.h>

#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkFontMgr_fuchsia.h"

namespace skia {

void InitializeSkFontMgrForTest() {
  OverrideDefaultSkFontMgr(
      SkFontMgr_New_Fuchsia(GetTestFontsProvider().BindSync()));
}

}  // namespace skia
