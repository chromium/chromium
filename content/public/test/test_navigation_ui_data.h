// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_NAVIGATION_UI_DATA_H_
#define CONTENT_PUBLIC_TEST_TEST_NAVIGATION_UI_DATA_H_

#include <memory>

#include "content/public/browser/navigation_ui_data.h"

namespace content {

// Subclass for content tests to check NavigationUIData is passed around
// correctly.
class TestNavigationUIData : public content::NavigationUIData {
 public:
  TestNavigationUIData() {}
  ~TestNavigationUIData() override {}

  std::unique_ptr<NavigationUIData> Clone() override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_NAVIGATION_UI_DATA_H_
