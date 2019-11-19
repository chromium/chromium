// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

TEST(DragUpdateTest, AffectedByDragUpdate) {
  // Check that when dragging the div in the document below, you only get a
  // single element style recalc.

  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.documentElement()->SetInnerHTMLFromString(R"HTML(
    <style>div {width:100px;height:100px} div:-webkit-drag {
    background-color: green }</style>
    <div id='div'>
    <span></span>
    <span></span>
    <span></span>
    <span></span>
    </div>
  )HTML");

  document.View()->UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);
  unsigned start_count = document.GetStyleEngine().StyleForElementCount();

  document.getElementById("div")->SetDragged(true);
  document.View()->UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);

  unsigned element_count =
      document.GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

TEST(DragUpdateTest, ChildAffectedByDragUpdate) {
  // Check that when dragging the div in the document below, you get a
  // single element style recalc.

  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.documentElement()->SetInnerHTMLFromString(R"HTML(
    <style>div {width:100px;height:100px} div:-webkit-drag .drag {
    background-color: green }</style>
    <div id='div'>
    <span></span>
    <span></span>
    <span class='drag'></span>
    <span></span>
    </div>
  )HTML");

  document.UpdateStyleAndLayout();
  unsigned start_count = document.GetStyleEngine().StyleForElementCount();

  document.getElementById("div")->SetDragged(true);
  document.UpdateStyleAndLayout();

  unsigned element_count =
      document.GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

TEST(DragUpdateTest, SiblingAffectedByDragUpdate) {
  // Check that when dragging the div in the document below, you get a
  // single element style recalc.

  auto dummy_page_holder = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.documentElement()->SetInnerHTMLFromString(R"HTML(
    <style>div {width:100px;height:100px} div:-webkit-drag + .drag {
    background-color: green }</style>
    <div id='div'>
    <span></span>
    <span></span>
    <span></span>
    <span></span>
    </div>
    <span class='drag'></span>
  )HTML");

  document.UpdateStyleAndLayout();
  unsigned start_count = document.GetStyleEngine().StyleForElementCount();

  document.getElementById("div")->SetDragged(true);
  document.UpdateStyleAndLayout();

  unsigned element_count =
      document.GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

}  // namespace blink
