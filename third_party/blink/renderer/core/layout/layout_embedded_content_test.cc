// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutEmbeddedContentTest : public RenderingTest {};

class OverriddenLayoutEmbeddedContent : public LayoutEmbeddedContent {
 public:
  explicit OverriddenLayoutEmbeddedContent(HTMLFrameOwnerElement* element)
      : LayoutEmbeddedContent(element) {}

  const char* GetName() const override {
    return "OverriddenLayoutEmbeddedContent";
  }
};

}  // namespace blink
