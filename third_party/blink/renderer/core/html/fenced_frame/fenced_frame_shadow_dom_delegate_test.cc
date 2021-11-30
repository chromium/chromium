// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class FencedFrameShadowDOMDelegateTest : public RenderingTest {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    base::FieldTrialParams params;
    params["implementation_type"] = "shadow_dom";
    enabled_feature_list_.InitAndEnableFeatureWithParameters(
        features::kFencedFrames, params);
  }

  HTMLFencedFrameElement& FencedFrame() {
    HTMLCollection* collection =
        GetDocument().getElementsByTagName("fencedframe");
    DCHECK(collection->HasExactlyOneItem());
    Element* element = *collection->begin();
    return To<HTMLFencedFrameElement>(*element);
  }

  void AssertInternalIFrameExists(HTMLFencedFrameElement* fenced_frame) {
    ShadowRoot* shadow_root = fenced_frame->UserAgentShadowRoot();
    EXPECT_TRUE(shadow_root);

    HTMLCollection* collection = shadow_root->getElementsByTagName("iframe");
    EXPECT_TRUE(collection->HasExactlyOneItem());
  }

  HTMLIFrameElement& ShadowIFrame() {
    HTMLFencedFrameElement& element = FencedFrame();
    DCHECK(element.InnerIFrameElement());
    return *element.InnerIFrameElement();
  }

  String IFrameHTMLAsString(HTMLFencedFrameElement& element) {
    return element.InnerIFrameElement()->outerHTML();
  }

 private:
  base::test::ScopedFeatureList enabled_feature_list_;
};

TEST_F(FencedFrameShadowDOMDelegateTest, CreateRaw) {
  HTMLFencedFrameElement* fenced_frame =
      MakeGarbageCollected<HTMLFencedFrameElement>(GetDocument());

  EXPECT_FALSE(fenced_frame->isConnected());
  EXPECT_NE(nullptr, fenced_frame->UserAgentShadowRoot());
  EXPECT_EQ("", fenced_frame->UserAgentShadowRoot()->innerHTML());

  GetDocument().body()->AppendChild(fenced_frame);
  EXPECT_TRUE(fenced_frame->isConnected());
  EXPECT_NE(nullptr, fenced_frame->UserAgentShadowRoot());
  EXPECT_EQ("<iframe></iframe>", IFrameHTMLAsString(*fenced_frame));
  EXPECT_TRUE(ShadowIFrame().GetFramePolicy().is_fenced);
  EXPECT_FALSE(fenced_frame->ShouldFreezeFrameSizeOnNextLayoutForTesting());
  EXPECT_FALSE(fenced_frame->FrozenFrameSize());
}

TEST_F(FencedFrameShadowDOMDelegateTest, CreateViasetInnerHTML) {
  GetDocument().body()->setInnerHTML("<fencedframe></fencedframe>");
  HTMLFencedFrameElement& fenced_frame = FencedFrame();
  EXPECT_TRUE(fenced_frame.isConnected());
  EXPECT_NE(nullptr, fenced_frame.UserAgentShadowRoot());
  EXPECT_EQ("<iframe></iframe>", IFrameHTMLAsString(fenced_frame));
  EXPECT_TRUE(ShadowIFrame().GetFramePolicy().is_fenced);
  EXPECT_FALSE(fenced_frame.ShouldFreezeFrameSizeOnNextLayoutForTesting());
  EXPECT_FALSE(fenced_frame.FrozenFrameSize());
}

TEST_F(FencedFrameShadowDOMDelegateTest, AppendRemoveAppend) {
  HTMLFencedFrameElement* fenced_frame =
      MakeGarbageCollected<HTMLFencedFrameElement>(GetDocument());
  EXPECT_EQ(0u, fenced_frame->UserAgentShadowRoot()->CountChildren());

  // Upon insertion of an HTMLFencedFrameElement, its
  // FencedFrameShadowDOMDelegate creates an internal <iframe>.
  GetDocument().body()->AppendChild(fenced_frame);
  AssertInternalIFrameExists(fenced_frame);

  // Upon removal of the HTMLFencedFrameElement, we do not delete the ShadowDOM
  // or its children.
  GetDocument().body()->RemoveChild(fenced_frame);
  AssertInternalIFrameExists(fenced_frame);

  // Upon re-insertion, we do not create *another* nested <iframe> since we
  // already have one under-the-hood.
  GetDocument().body()->AppendChild(fenced_frame);
  AssertInternalIFrameExists(fenced_frame);
}

TEST_F(FencedFrameShadowDOMDelegateTest, PresentationAttributes) {
  SetBodyInnerHTML(R"HTML(
    <fencedframe width="123" height="456"></fencedframe>
)HTML");
  HTMLFencedFrameElement& fenced_frame = FencedFrame();
  const auto* box = fenced_frame.GetLayoutBox();
  ASSERT_TRUE(box);
  EXPECT_EQ(box->StyleRef().Width(), Length::Fixed(123));
  EXPECT_EQ(box->StyleRef().Height(), Length::Fixed(456));
  EXPECT_EQ(box->OffsetWidth(), LayoutUnit(123));
  EXPECT_EQ(box->OffsetHeight(), LayoutUnit(456));
}

// This test tests navigations with respect to the DOM-connectedness of the
// element.
TEST_F(FencedFrameShadowDOMDelegateTest, NavigationWithInsertionAndRemoval) {
  HTMLFencedFrameElement* fenced_frame =
      MakeGarbageCollected<HTMLFencedFrameElement>(GetDocument());
  EXPECT_EQ(0u, fenced_frame->UserAgentShadowRoot()->CountChildren());
  EXPECT_FALSE(fenced_frame->ShouldFreezeFrameSizeOnNextLayoutForTesting());
  EXPECT_FALSE(fenced_frame->FrozenFrameSize());

  // Navigation before insertion has no effect.
  fenced_frame->setAttribute(html_names::kSrcAttr, "https://example.com");
  EXPECT_EQ(0u, fenced_frame->UserAgentShadowRoot()->CountChildren());
  EXPECT_FALSE(fenced_frame->ShouldFreezeFrameSizeOnNextLayoutForTesting());
  EXPECT_FALSE(fenced_frame->FrozenFrameSize());

  // Insertion causes navigation to the last `src` attribute mutation value.
  GetDocument().body()->AppendChild(fenced_frame);
  AssertInternalIFrameExists(fenced_frame);
  EXPECT_EQ("<iframe src=\"https://example.com/\"></iframe>",
            IFrameHTMLAsString(*fenced_frame));
  EXPECT_TRUE(fenced_frame->ShouldFreezeFrameSizeOnNextLayoutForTesting());
  EXPECT_FALSE(fenced_frame->FrozenFrameSize());

  // Navigating after insertion works correctly.
  fenced_frame->setAttribute(html_names::kSrcAttr, "https://example-2.com");
  EXPECT_EQ("<iframe src=\"https://example-2.com/\"></iframe>",
            IFrameHTMLAsString(*fenced_frame));
  EXPECT_TRUE(ShadowIFrame().GetFramePolicy().is_fenced);

  // Removal does not remove the internal iframe, or change its `src`.
  GetDocument().body()->RemoveChild(fenced_frame);
  AssertInternalIFrameExists(fenced_frame);
  EXPECT_EQ("<iframe src=\"https://example-2.com/\"></iframe>",
            IFrameHTMLAsString(*fenced_frame));

  // Navigating after removal has no effect on the internal iframe's `src`
  // attribute, because the HTMLFencedFrameElement is not connected.
  fenced_frame->setAttribute(html_names::kSrcAttr, "https://example-3.com");
  EXPECT_EQ("<iframe src=\"https://example-2.com/\"></iframe>",
            IFrameHTMLAsString(*fenced_frame));

  // Re-insertion allows the last `src` attribute mutation to take effect.
  GetDocument().body()->AppendChild(fenced_frame);
  AssertInternalIFrameExists(fenced_frame);
  EXPECT_EQ("<iframe src=\"https://example-3.com/\"></iframe>",
            IFrameHTMLAsString(*fenced_frame));
  EXPECT_TRUE(ShadowIFrame().GetFramePolicy().is_fenced);
}

TEST_F(FencedFrameShadowDOMDelegateTest, FreezeSizeByInnerHTML) {
  SetBodyInnerHTML(R"HTML(
    <style>
    fencedframe {
      width: 100px;
      height: 100px;
    }
    .large {
      width: 200px;
      height: 200px;
    }
    </style>
    <fencedframe src="https://example.com"></fencedframe>
)HTML");
  // The frame size should be frozen because the `src` attribute is set.
  HTMLFencedFrameElement& fenced_frame = FencedFrame();
  EXPECT_EQ(fenced_frame.FrozenFrameSize(), PhysicalSize(100, 100));

  // Changing the size of the fenced frame should not affect the frozen size.
  fenced_frame.classList().Add("large");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(fenced_frame.FrozenFrameSize(), PhysicalSize(100, 100));
}

TEST_F(FencedFrameShadowDOMDelegateTest, FreezeSizeBySrc) {
  SetBodyInnerHTML(R"HTML(
    <style>
    fencedframe {
      width: 100px;
      height: 100px;
    }
    .large {
      width: 200px;
      height: 200px;
    }
    </style>
    <fencedframe></fencedframe>
)HTML");
  HTMLFencedFrameElement& fenced_frame = FencedFrame();
  EXPECT_FALSE(fenced_frame.ShouldFreezeFrameSizeOnNextLayoutForTesting());
  EXPECT_FALSE(fenced_frame.FrozenFrameSize());

  fenced_frame.classList().Add("large");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(fenced_frame.FrozenFrameSize());

  // The frame size is frozen after the fenced frame became larger.
  fenced_frame.setAttribute(html_names::kSrcAttr, "https://example.com");
  EXPECT_EQ(fenced_frame.FrozenFrameSize(), PhysicalSize(200, 200));

  // Making it smaller should not affect the frozen size.
  fenced_frame.classList().Remove("large");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(fenced_frame.FrozenFrameSize(), PhysicalSize(200, 200));
}

// Test when the frozen width or height is zero.
// The `object-fit: contain` behavior can't scale the content if its width or
// height is zero. It shouldn't hit divide-by-zero errors in such cases.
TEST_F(FencedFrameShadowDOMDelegateTest, FreezeSizeToZero) {
  SetBodyInnerHTML(R"HTML(
    <style>
    fencedframe {
      width: 100px;
      height: 0;
    }
    </style>
    <fencedframe src="https://example.com"></fencedframe>
)HTML");
  HTMLFencedFrameElement& fenced_frame = FencedFrame();
  EXPECT_EQ(fenced_frame.FrozenFrameSize(), PhysicalSize(100, 0));
}

TEST_F(FencedFrameShadowDOMDelegateTest, FreezeSizeWithBorderPadding) {
  SetBodyInnerHTML(R"HTML(
    <style>
    fencedframe {
      width: 100px;
      height: 100px;
      border: 15px solid blue;
      padding: 10px;
    }
    </style>
    <fencedframe src="https://example.com"></fencedframe>
)HTML");
  // The frame size should be frozen because the `src` attribute is set.
  HTMLFencedFrameElement& fenced_frame = FencedFrame();
  EXPECT_EQ(fenced_frame.FrozenFrameSize(), PhysicalSize(100, 100));
}

}  // namespace blink
