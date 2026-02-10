// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_credential_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class HTMLCredentialElementTest : public PageTestBase {};

TEST_F(HTMLCredentialElementTest, TagName) {
  auto* element = MakeGarbageCollected<HTMLCredentialElement>(GetDocument());
  EXPECT_EQ(element->tagName(), "CREDENTIAL");
  EXPECT_EQ(element->localName(), "credential");
}

}  // namespace blink
