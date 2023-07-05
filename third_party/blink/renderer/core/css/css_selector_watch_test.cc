// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_selector_watch.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CSSSelectorWatchTest : public PageTestBase {
 protected:
  StyleEngine& GetStyleEngine() { return GetDocument().GetStyleEngine(); }

  static const HashSet<String> AddedSelectors(const CSSSelectorWatch& watch) {
    return watch.added_selectors_;
  }
  static const HashSet<String> RemovedSelectors(const CSSSelectorWatch& watch) {
    return watch.removed_selectors_;
  }
  static void ClearAddedRemoved(CSSSelectorWatch&);
};

void CSSSelectorWatchTest::ClearAddedRemoved(CSSSelectorWatch& watch) {
  watch.added_selectors_.clear();
  watch.removed_selectors_.clear();
}

TEST_F(CSSSelectorWatchTest, RecalcOnDocumentChange) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div>
      <span id='x' class='a'></span>
      <span id='y' class='b'><span></span></span>
      <span id='z'><span></span></span>
    </div>
  )HTML");

  CSSSelectorWatch& watch = CSSSelectorWatch::From(GetDocument());

  Vector<String> selectors;
  selectors.push_back(".a");
  watch.WatchCSSSelectors(selectors);

  UpdateAllLifecyclePhasesForTest();

  selectors.clear();
  selectors.push_back(".b");
  selectors.push_back(".c");
  selectors.push_back("#nomatch");
  watch.WatchCSSSelectors(selectors);

  UpdateAllLifecyclePhasesForTest();

  Element* x = GetDocument().getElementById(AtomicString("x"));
  Element* y = GetDocument().getElementById(AtomicString("y"));
  Element* z = GetDocument().getElementById(AtomicString("z"));
  ASSERT_TRUE(x);
  ASSERT_TRUE(y);
  ASSERT_TRUE(z);

  x->removeAttribute(html_names::kClassAttr);
  y->removeAttribute(html_names::kClassAttr);
  z->setAttribute(html_names::kClassAttr, AtomicString("c"));

  ClearAddedRemoved(watch);

  unsigned before_count = GetStyleEngine().StyleForElementCount();
  UpdateAllLifecyclePhasesForTest();
  unsigned after_count = GetStyleEngine().StyleForElementCount();

  EXPECT_EQ(2u, after_count - before_count);

  EXPECT_EQ(1u, AddedSelectors(watch).size());
  EXPECT_TRUE(AddedSelectors(watch).Contains(".c"));

  EXPECT_EQ(1u, RemovedSelectors(watch).size());
  EXPECT_TRUE(RemovedSelectors(watch).Contains(".b"));
}

class CSSSelectorWatchCQTest : public CSSSelectorWatchTest {
 protected:
  CSSSelectorWatchCQTest() = default;
};

TEST_F(CSSSelectorWatchCQTest, ContainerQueryDisplayNone) {
  CSSSelectorWatch& watch = CSSSelectorWatch::From(GetDocument());

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      .c #container {
        container-name: c1;
        container-type: inline-size;
      }
      .c #inner { display: none; }
      @container c1 (min-width: 200px) {
        .c #inner { display: inline }
      }
    </style>
    <div id="container">
      <span id="inner"></span>
    </div>
  )HTML");

  Vector<String> selectors;
  selectors.push_back("#inner");
  watch.WatchCSSSelectors(selectors);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(1u, AddedSelectors(watch).size());
  EXPECT_TRUE(AddedSelectors(watch).Contains("#inner"));
  EXPECT_EQ(0u, RemovedSelectors(watch).size());

  // Setting the class 'c' on body will make #inner display:none, but also make
  // #container a container 'c1' which is flipping the span back to
  // display:inline.
  ClearAddedRemoved(watch);
  GetDocument().body()->setAttribute(html_names::kClassAttr, AtomicString("c"));
  UpdateAllLifecyclePhasesForTest();

  // Element::UpdateCallbackSelectors() will both remove and add #inner in the
  // two passes. First without the CQ matching, and then in an interleaved style
  // and layout pass. The accounting in CSSSelectorWatch::UpdateSelectorMatches
  // will make sure we up with a zero balance.
  EXPECT_EQ(0u, AddedSelectors(watch).size());
  EXPECT_EQ(0u, RemovedSelectors(watch).size());
}

}  // namespace blink
