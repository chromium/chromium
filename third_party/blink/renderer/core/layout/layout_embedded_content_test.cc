// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"

#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

class LayoutFreezableIFrame : public LayoutIFrame {
 public:
  explicit LayoutFreezableIFrame(HTMLFrameOwnerElement* element)
      : LayoutIFrame(element) {}

  void FreezeSizeForTesting(const PhysicalSize& size) {
    NOT_DESTROYED();
    frozen_size_ = size;
    SetNeedsLayoutAndFullPaintInvalidation("test");
  }

 protected:
  const std::optional<PhysicalSize> FrozenFrameSize() const override {
    NOT_DESTROYED();
    return frozen_size_;
  }

 private:
  std::optional<PhysicalSize> frozen_size_;
};

class HTMLFreezableIFrameElement : public HTMLIFrameElement {
 public:
  explicit HTMLFreezableIFrameElement(Document& document)
      : HTMLIFrameElement(document) {}

  LayoutFreezableIFrame* GetLayoutFreezableIFrame() const {
    return static_cast<LayoutFreezableIFrame*>(GetLayoutObject());
  }

 private:
  bool LayoutObjectIsNeeded(const DisplayStyle&) const override { return true; }
  LayoutObject* CreateLayoutObject(const ComputedStyle&) override {
    return MakeGarbageCollected<LayoutFreezableIFrame>(this);
  }
};

}  // namespace

class LayoutEmbeddedContentTest : public RenderingTest {};

TEST_F(LayoutEmbeddedContentTest, FreozenSizeReplacedContentRect) {
  Document& document = GetDocument();
  auto* element = MakeGarbageCollected<HTMLFreezableIFrameElement>(document);
  element->setAttribute(html_names::kSrcAttr,
                        AtomicString("http://example.com/"));
  element->SetInlineStyleProperty(CSSPropertyID::kObjectFit,
                                  CSSValueID::kContain);
  document.body()->AppendChild(element);
  UpdateAllLifecyclePhasesForTest();
  auto* layout_object = element->GetLayoutFreezableIFrame();
  ASSERT_TRUE(layout_object);
  EXPECT_EQ(layout_object->ReplacedContentRect(), PhysicalRect(2, 2, 300, 150));

  layout_object->FreezeSizeForTesting(PhysicalSize(80, 50));
  UpdateAllLifecyclePhasesForTest();
  // When the size is frozen, the content is rendered at the centre of the box
  // and scale to fit based on object-fit:contain.
  EXPECT_EQ(layout_object->ReplacedContentRect(),
            PhysicalRect(32, 2, 240, 150));
}

TEST_F(LayoutEmbeddedContentTest, FreozenSizeEmpty) {
  Document& document = GetDocument();
  auto* element = MakeGarbageCollected<HTMLFreezableIFrameElement>(document);
  element->setAttribute(html_names::kSrcAttr,
                        AtomicString("http://example.com/"));
  element->SetInlineStyleProperty(CSSPropertyID::kObjectFit,
                                  CSSValueID::kContain);
  document.body()->AppendChild(element);
  UpdateAllLifecyclePhasesForTest();
  auto* layout_object = element->GetLayoutFreezableIFrame();
  ASSERT_TRUE(layout_object);
  EXPECT_EQ(layout_object->ReplacedContentRect(), PhysicalRect(2, 2, 300, 150));

  layout_object->FreezeSizeForTesting(PhysicalSize(0, 10));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(layout_object->ReplacedContentRect(), PhysicalRect(2, 2, 300, 150));

  layout_object->FreezeSizeForTesting(PhysicalSize(10, 0));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(layout_object->ReplacedContentRect(), PhysicalRect(2, 2, 300, 150));
}

}  // namespace blink
