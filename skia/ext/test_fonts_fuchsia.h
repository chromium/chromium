// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_TEST_FONTS_FUCHSIA_H_
#define SKIA_EXT_TEST_FONTS_FUCHSIA_H_

#include <fuchsia/fonts/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>

namespace skia {

fuchsia::fonts::ProviderSyncPtr RunTestProviderWithTestFonts(
    fidl::InterfaceHandle<fuchsia::sys::ComponentController>* controller_out);

}  // namespace skia

#endif  // SKIA_EXT_TEST_FONTS_FUCHSIA_H_
