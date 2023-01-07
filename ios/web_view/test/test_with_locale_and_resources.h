// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_TEST_TEST_WITH_LOCALE_AND_RESOURCES_H_
#define IOS_WEB_VIEW_TEST_TEST_WITH_LOCALE_AND_RESOURCES_H_

#include "testing/platform_test.h"

namespace ios_web_view {

// A test suite that ensure the locale is correctly set and the resources
// are available. Use this base class for tests instead of PlatformTest if
// they depend on localized strings.
class TestWithLocaleAndResources : public PlatformTest {
 protected:
  TestWithLocaleAndResources();
  ~TestWithLocaleAndResources() override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_TEST_TEST_WITH_LOCALE_AND_RESOURCES_H_
