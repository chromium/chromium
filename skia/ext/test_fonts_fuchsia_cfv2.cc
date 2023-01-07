// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/test_fonts_fuchsia.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>

#include "base/fuchsia/process_context.h"

namespace skia {

fuchsia::fonts::ProviderHandle GetTestFontsProvider() {
  // //build/config/fuchsia/test/test_fonts.shard.test-cml must be in the
  // current test component's manifest. It configures a fonts.Provider to serve
  // fonts from the package's test_fonts directory for the test process.
  fuchsia::fonts::ProviderHandle provider;
  base::ComponentContextForProcess()->svc()->Connect(provider.NewRequest());
  return provider;
}

}  // namespace skia
