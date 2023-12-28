// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(DragUpdateTest, AffectedByDragUpdate) {
  test::TaskEnvironment task_environment;
  // Check that when dragging the div in the document below, you only get a
  // single element style recalc.

  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.documentElement()->setInnerHTML(R"HTML(
    <style>div {width:100px;height:100px} div:-webkit-drag {
    background-color: green }</style>
    <div id='div'>
    <span></span>
    <span></span>
    <span></span>
    <span></span>
    </div>
  )HTML");

  document.View()->UpdateAllLifecyclePhasesForTest();
  unsigned start_count = document.GetStyleEngine().StyleForElementCount();

  document.getElementById(AtomicString("div"))->SetDragged(true);
  document.View()->UpdateAllLifecyclePhasesForTest();

  unsigned element_count =
      document.GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

TEST(DragUpdateTest, ChildAffectedByDragUpdate) {
  test::TaskEnvironment task_environment;
  // Check that when dragging the div in the document below, you get a
  // single element style recalc.

  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.documentElement()->setInnerHTML(R"HTML(
    <style>div {width:100px;height:100px} div:-webkit-drag .drag {
    background-color: green }</style>
    <div id='div'>
    <span></span>
    <span></span>
    <span class='drag'></span>
    <span></span>
    </div>
  )HTML");

  document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  unsigned start_count = document.GetStyleEngine().StyleForElementCount();

  document.getElementById(AtomicString("div"))->SetDragged(true);
  document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  unsigned element_count =
      document.GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

TEST(DragUpdateTest, SiblingAffectedByDragUpdate) {
  test::TaskEnvironment task_environment;
  // Check that when dragging the div in the document below, you get a
  // single element style recalc.

  auto dummy_page_holder =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder->GetDocument();
  document.documentElement()->setInnerHTML(R"HTML(
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

  document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  unsigned start_count = document.GetStyleEngine().StyleForElementCount();

  document.getElementById(AtomicString("div"))->SetDragged(true);
  document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  unsigned element_count =
      document.GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

}  // namespace blink
