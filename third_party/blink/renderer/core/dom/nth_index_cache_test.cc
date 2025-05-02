// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/nth_index_cache.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class NthIndexCacheTest : public PageTestBase {};

TEST_F(NthIndexCacheTest, NthIndex) {
  GetDocument().documentElement()->setInnerHTML(R"HTML(
    <body>
    <span
    id=first></span><span></span><span></span><span></span><span></span>
    <span></span><span></span><span></span><span></span><span></span>
    Text does not count
    <span id=nth-last-child></span>
    <span id=nth-child></span>
    <span></span><span></span><span></span><span></span><span></span>
    <span></span><span></span><span></span><span></span><span
    id=last></span>
    </body>
  )HTML");

  NthIndexCache nth_index_cache(GetDocument());

  EXPECT_EQ(nth_index_cache.NthChildIndex(
                *GetElementById("nth-child"), /*filter=*/nullptr,
                /*selector_checker=*/nullptr, /*context=*/nullptr),
            12U);
  EXPECT_EQ(nth_index_cache.NthLastChildIndex(
                *GetElementById("nth-last-child"), /*filter=*/nullptr,
                /*selector_checker=*/nullptr, /*context=*/nullptr),
            12U);
}

}  // namespace blink
