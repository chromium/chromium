// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/events/event_path.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class EventPathTest : public PageTestBase {};

TEST_F(EventPathTest, ShouldBeEmptyForPseudoElementWithoutParentElement) {
  Element* div = GetDocument().CreateRawElement(
      html_names::kDivTag, CreateElementFlags::ByCreateElement());
  PseudoElement* pseudo = PseudoElement::Create(div, kPseudoIdFirstLetter);
  pseudo->Dispose();
  EventPath* event_path = MakeGarbageCollected<EventPath>(*pseudo);
  EXPECT_TRUE(event_path->IsEmpty());
}

}  // namespace blink
