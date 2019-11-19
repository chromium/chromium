// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/fontmgr_default.h"

#include <fuchsia/fonts/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/default_context.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkFontMgr_fuchsia.h"

namespace skia {

SK_API sk_sp<SkFontMgr> CreateDefaultSkFontMgr() {
  fuchsia::fonts::ProviderSyncPtr provider;
  base::fuchsia::ComponentContextForCurrentProcess()->svc()->Connect(
      provider.NewRequest());
  return SkFontMgr_New_Fuchsia(std::move(provider));
}

}  // namespace skia