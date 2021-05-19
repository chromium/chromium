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
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class FencedFrameShadowDOMDelegateTest : public PageTestBase {
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
    EXPECT_TRUE(shadow_root->Children()->HasExactlyOneItem());

    HTMLCollection* collection = shadow_root->getElementsByTagName("iframe");
    EXPECT_TRUE(collection->HasExactlyOneItem());
  }

  HTMLIFrameElement& ShadowIFrame() {
    HTMLCollection* collection =
        FencedFrame().UserAgentShadowRoot()->Children();
    DCHECK(collection->HasExactlyOneItem());
    Element* element = *collection->begin();
    return To<HTMLIFrameElement>(*element);
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
  EXPECT_EQ("<iframe></iframe>",
            fenced_frame->UserAgentShadowRoot()->innerHTML());
}

TEST_F(FencedFrameShadowDOMDelegateTest, CreateViasetInnerHTML) {
  GetDocument().body()->setInnerHTML("<fencedframe></fencedframe>");
  HTMLFencedFrameElement& fenced_frame = FencedFrame();
  EXPECT_TRUE(fenced_frame.isConnected());
  EXPECT_NE(nullptr, fenced_frame.UserAgentShadowRoot());
  EXPECT_EQ("<iframe></iframe>",
            fenced_frame.UserAgentShadowRoot()->innerHTML());
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

// This test tests navigations with respect to the DOM-connectedness of the
// element.
TEST_F(FencedFrameShadowDOMDelegateTest, NavigationWithInsertionAndRemoval) {
  HTMLFencedFrameElement* fenced_frame =
      MakeGarbageCollected<HTMLFencedFrameElement>(GetDocument());
  EXPECT_EQ(0u, fenced_frame->UserAgentShadowRoot()->CountChildren());

  // Navigation before insertion has no effect.
  fenced_frame->setAttribute(html_names::kSrcAttr, "https://example.com");
  EXPECT_EQ(0u, fenced_frame->UserAgentShadowRoot()->CountChildren());

  // Insertion causes navigation to the last `src` attribute mutation value.
  GetDocument().body()->AppendChild(fenced_frame);
  AssertInternalIFrameExists(fenced_frame);
  EXPECT_EQ("<iframe src=\"https://example.com/\"></iframe>",
            fenced_frame->UserAgentShadowRoot()->innerHTML());

  // Navigating after insertion works correctly.
  fenced_frame->setAttribute(html_names::kSrcAttr, "https://example-2.com");
  EXPECT_EQ("<iframe src=\"https://example-2.com/\"></iframe>",
            fenced_frame->UserAgentShadowRoot()->innerHTML());

  // Removal does not remove the internal iframe, or change its `src`.
  GetDocument().body()->RemoveChild(fenced_frame);
  AssertInternalIFrameExists(fenced_frame);
  EXPECT_EQ("<iframe src=\"https://example-2.com/\"></iframe>",
            fenced_frame->UserAgentShadowRoot()->innerHTML());

  // Navigating after removal has no effect on the internal iframe's `src`
  // attribute, because the HTMLFencedFrameElement is not connected.
  fenced_frame->setAttribute(html_names::kSrcAttr, "https://example-3.com");
  EXPECT_EQ("<iframe src=\"https://example-2.com/\"></iframe>",
            fenced_frame->UserAgentShadowRoot()->innerHTML());

  // Re-insertion allows the last `src` attribute mutation to take effect.
  GetDocument().body()->AppendChild(fenced_frame);
  AssertInternalIFrameExists(fenced_frame);
  EXPECT_EQ("<iframe src=\"https://example-3.com/\"></iframe>",
            fenced_frame->UserAgentShadowRoot()->innerHTML());
}

}  // namespace blink
