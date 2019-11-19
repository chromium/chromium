// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_query_matcher.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

TEST(MediaQueryMatcherTest, LostFrame) {
  auto page_holder = std::make_unique<DummyPageHolder>(IntSize(500, 500));
  auto* matcher =
      MakeGarbageCollected<MediaQueryMatcher>(page_holder->GetDocument());
  scoped_refptr<MediaQuerySet> query_set =
      MediaQuerySet::Create(media_type_names::kAll);
  ASSERT_TRUE(matcher->Evaluate(query_set.get()));

  matcher->DocumentDetached();
  ASSERT_FALSE(matcher->Evaluate(query_set.get()));
}

}  // namespace blink
