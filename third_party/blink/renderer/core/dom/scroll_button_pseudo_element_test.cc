// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_button_pseudo_element.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

using ScrollButtonPseudoElementTest = PageTestBase;

TEST_F(ScrollButtonPseudoElementTest, ScrollButtonsRetainFocus) {
  SetBodyContent(R"HTML(
  <style>
  #scroller {
    width: 600px;
    height: 300px;
    overflow: auto;
    scroll-marker-group: after;
    white-space: nowrap;
  }
  #scroller div {
    background: green;
    display: inline-block;
    width: 600px;
    height: 270px;
  }
  #scroller :first-child {
    background: purple;
  }
  #scroller::scroll-marker-group {
    border: 3px solid black;
    padding: 5px;
    display: flex;
    height: 20px;
    width: 40px;
  }
  #scroller::scroll-button(right) {
    content: ">";
    background: blue;
    display: flex;
    height: 20px;
    width: 20px;
    top: 0px;
    left: 0px;
    position: absolute;
    z-index: 99;
  }
  #scroller div::scroll-marker {
    content: "";
    width: 10px;
    height: 10px;
    background-color: blue;
    border-radius: 100%;
    display: inline-block;
  }
  </style>
  <div id="scroller">
    <div></div>
    <div></div>
  </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* body = GetDocument().body();
  Element* scroller = body->QuerySelector(AtomicString("#scroller"));
  auto* scroll_button = To<ScrollButtonPseudoElement>(
      scroller->GetPseudoElement(kPseudoIdScrollButtonInlineEnd));
  MouseEventInit& mouse_event_init = *MouseEventInit::Create();
  auto* mouse_event = MakeGarbageCollected<MouseEvent>(event_type_names::kClick,
                                                       &mouse_event_init);
  mouse_event->SetTarget(scroll_button);
  scroll_button->DefaultEventHandler(*mouse_event);
  EXPECT_TRUE(scroll_button->IsFocused());
}

}  // namespace blink
