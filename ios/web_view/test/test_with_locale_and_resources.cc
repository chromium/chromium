// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/test/test_with_locale_and_resources.h"

#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

namespace ios_web_view {

TestWithLocaleAndResources::TestWithLocaleAndResources() {
  l10n_util::OverrideLocaleWithCocoaLocale();
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      l10n_util::GetLocaleOverride(), /*delegate=*/nullptr,
      ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
}

TestWithLocaleAndResources::~TestWithLocaleAndResources() {
  ui::ResourceBundle::CleanupSharedInstance();
}

}  // namespace ios_web_view
