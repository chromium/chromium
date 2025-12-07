// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/unique_font_selector.h"

#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_font_cache.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class UniqueFontSelectorTest : public PageTestBase {
 protected:
  static wtf_size_t CacheSizeOf(const UniqueFontSelector& selector) {
    return selector.font_cache_.size();
  }
};

TEST_F(UniqueFontSelectorTest, CacheSizeLimit) {
  auto* selector = MakeGarbageCollected<UniqueFontSelector>(
      GetDocument().GetStyleEngine().GetFontSelector());
  FontDescription desc;
  const wtf_size_t max_size = CanvasFontCache::MaxFonts();

  // We get max_size * 3 different fonts.
  for (wtf_size_t size = 0; size < max_size * 3; ++size) {
    desc.SetComputedSize(size);
    selector->FindOrCreateFont(desc);
  }
  // The cache should have max_size * 2 entries, not max_size * 3.
  EXPECT_EQ(max_size * 2, CacheSizeOf(*selector));

  selector->DidSwitchFrame();
  // Get a new font after switching frame. The cache size should shrink
  // to max_size.
  desc.SetComputedSize(desc.ComputedSize() + 1);
  selector->FindOrCreateFont(desc);
  EXPECT_EQ(max_size, CacheSizeOf(*selector));
}

}  // namespace blink
