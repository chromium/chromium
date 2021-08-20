// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webdatabase/dom_window_web_database.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

TEST(DOMWindowWebDatabaseTest, IsThirdPartyContextWebSQLDeprecated) {
  EXPECT_EQ(DOMWindowWebDatabase::IsThirdPartyContextWebSQLDeprecated(), false);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kDeprecateThirdPartyContextWebSQL);
  EXPECT_EQ(DOMWindowWebDatabase::IsThirdPartyContextWebSQLDeprecated(), true);
}

}  // namespace blink
