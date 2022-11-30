// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_TEST_FONTS_H_
#define SKIA_EXT_TEST_FONTS_H_

namespace skia {

// Configures the process to use //third_party/test_fonts. Should be called
// early, before default instance of SkFontMgr is created.
void InitializeSkFontMgrForTest();

}  // namespace skia

#endif  // SKIA_EXT_TEST_FONTS_H_
