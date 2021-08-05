// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/optimization_guide/tab_url_provider_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class TabUrlProviderImplTest : public PlatformTest {
 public:
  TabUrlProviderImplTest() {
    tab_url_provider_ = std::make_unique<TabUrlProviderImpl>();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TabUrlProviderImpl> tab_url_provider_;
};

}  // namespace
