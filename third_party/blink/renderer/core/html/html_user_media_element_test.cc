// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_user_media_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class HTMLUserMediaElementTest : public PageTestBase {};

TEST_F(HTMLUserMediaElementTest, BranchingLogicBasedOnTypeAttribute) {
  auto* element = MakeGarbageCollected<HTMLUserMediaElement>(GetDocument());

  // Default state (New capability mode)
  EXPECT_FALSE(element->IsLegacyMode());

  // Set type to simulate legacy mode
  element->setAttribute(html_names::kTypeAttr, AtomicString("camera"));
  EXPECT_TRUE(element->IsLegacyMode());

  // Remove type to revert to new capability mode
  element->removeAttribute(html_names::kTypeAttr);
  EXPECT_FALSE(element->IsLegacyMode());
}

}  // namespace blink
