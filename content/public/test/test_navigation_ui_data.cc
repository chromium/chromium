// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_navigation_ui_data.h"

#include <memory>

namespace content {

std::unique_ptr<NavigationUIData> TestNavigationUIData::Clone() {
  return std::make_unique<TestNavigationUIData>();
}

}  // namespace content
