// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/public/test/render_view_test.h"
#include "ui/native_theme/native_theme_features.h"

namespace content {
class FluentScrollbarImplTest : public RenderViewTest {
 public:
  explicit FluentScrollbarImplTest() {
    feature_list_.InitAndEnableFeature(features::kFluentScrollbar);
  }

  ~FluentScrollbarImplTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Ensures that RenderViewTest based tests can properly initialize when Fluent
// scrollbars are enabled.
TEST_F(FluentScrollbarImplTest, FluentScrollbarsInitializeProperly) {
  EXPECT_TRUE(ui::IsFluentScrollbarEnabled());
}
}  // namespace content
