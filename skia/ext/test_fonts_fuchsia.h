// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_TEST_FONTS_FUCHSIA_H_
#define SKIA_EXT_TEST_FONTS_FUCHSIA_H_

#include <fuchsia/fonts/cpp/fidl.h>

namespace skia {

// Returns a handle to a fuchsia.fonts.Provider that serves the fonts in
// the package's test_fonts directory.
fuchsia::fonts::ProviderHandle GetTestFontsProvider();

}  // namespace skia

#endif  // SKIA_EXT_TEST_FONTS_FUCHSIA_H_
