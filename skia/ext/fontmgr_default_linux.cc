// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/fontmgr_default.h"

#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"
#include "third_party/skia/include/ports/SkFontMgr_FontConfigInterface.h"

namespace skia {

SK_API sk_sp<SkFontMgr> CreateDefaultSkFontMgr() {
  sk_sp<SkFontConfigInterface> fci(SkFontConfigInterface::RefGlobal());
  return fci ? SkFontMgr_New_FCI(std::move(fci)) : nullptr;
}

}  // namespace skia