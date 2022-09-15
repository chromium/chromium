// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/fontmgr_default.h"

#include "third_party/skia/include/core/SkFontMgr.h"

namespace {

SkDEBUGCODE(bool g_factory_called;)

    // This is a purposefully leaky pointer that has ownership of the FontMgr.
    SkFontMgr* g_fontmgr_override = nullptr;

}  // namespace

namespace skia {

void OverrideDefaultSkFontMgr(sk_sp<SkFontMgr> fontmgr) {
  SkASSERT(!g_factory_called);

  SkSafeUnref(g_fontmgr_override);
  g_fontmgr_override = fontmgr.release();
}

}  // namespace skia

SK_API sk_sp<SkFontMgr> SkFontMgr::Factory() {
  SkDEBUGCODE(g_factory_called = true;);

  return g_fontmgr_override ? sk_ref_sp(g_fontmgr_override)
                            : skia::CreateDefaultSkFontMgr();
}