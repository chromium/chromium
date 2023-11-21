// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/public/test/render_view_test.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "ui/native_theme/native_theme_features.h"

namespace content {

class FluentOverlayScrollbarImplTest : public RenderViewTest {
 public:
  explicit FluentOverlayScrollbarImplTest() {
    feature_list_.InitAndEnableFeature(features::kFluentOverlayScrollbar);
  }

  ~FluentOverlayScrollbarImplTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Ensures that RenderViewTest based tests can properly initialize when Fluent
// scrollbars are enabled. At one point RenderViewTest's ordering of platform vs
// NativeThemeFluent initialization would fail when fluent scrollbars were
// enabled. See https://crrev.com/c/4257851 for more details.
TEST_F(FluentOverlayScrollbarImplTest,
       FluentOverlayScrollbarsInitializeProperly) {
  blink::WebThemeEngine* theme_engine =
      blink::Platform::Current()->ThemeEngine();
  EXPECT_EQ(theme_engine->IsFluentOverlayScrollbarEnabled(),
            ui::IsFluentScrollbarEnabled());
}

}  // namespace content
