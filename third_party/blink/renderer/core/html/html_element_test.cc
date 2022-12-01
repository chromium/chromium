// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class HTMLElementTest : public RenderingTest {
 public:
  HTMLElementTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

TEST_F(HTMLElementTest, AdjustDirectionalityInFlatTree) {
  SetBodyContent("<bdi><summary><i id=target></i></summary></bdi>");
  UpdateAllLifecyclePhasesForTest();
  GetDocument().getElementById("target")->remove();
  // Pass if not crashed.
}

TEST_F(HTMLElementTest, DirectStyleMutationTriggered) {
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML("<div id='box'></div>");
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var box = document.getElementById('box');
    box.style.width = '100px';
    box.style.height = '100px';
  )JS");
  GetDocument().body()->appendChild(script);
  EXPECT_TRUE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  RunDocumentLifecycle();
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
}

TEST_F(HTMLElementTest, DirectStyleMutationNotTriggeredOnFirstFrameInDOM) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var box = document.createElement('box');
    document.body.appendChild(box);
    box.style.width = '100px';
    box.style.height = '100px';
  )JS");
  GetDocument().body()->appendChild(script);
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
}

TEST_F(HTMLElementTest, DirectStyleMutationNotTriggeredOnFirstPaint) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetBodyInnerHTML("<div id='box' style='display:none'></div>");
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  RunDocumentLifecycle();
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var box = document.getElementById('box');
    box.style.display = 'block';
    box.style.width = '100px';
    box.style.height = '100px';
  )JS");
  GetDocument().body()->appendChild(script);
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
}

TEST_F(HTMLElementTest, DirectStyleMutationFromString) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetBodyInnerHTML("<div id='box'></div>");
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  RunDocumentLifecycle();
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var box = document.getElementById('box');
    box.style = 'width:100px';
  )JS");
  GetDocument().body()->appendChild(script);
  EXPECT_TRUE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  RunDocumentLifecycle();
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
}

TEST_F(HTMLElementTest, DirectStyleMutationCustomPropertyFromString) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetBodyInnerHTML("<div id='box'></div>");
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  RunDocumentLifecycle();
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var box = document.getElementById('box');
    box.style = '--foo:100px';
  )JS");
  GetDocument().body()->appendChild(script);
  EXPECT_TRUE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  RunDocumentLifecycle();
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
}

TEST_F(HTMLElementTest, DirectStyleMutationByAttributeStyleMap) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetBodyInnerHTML("<div id='box'></div>");
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  RunDocumentLifecycle();
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var box = document.getElementById('box');
    box.attributeStyleMap.set('width', '100px');
  )JS");
  GetDocument().body()->appendChild(script);
  EXPECT_TRUE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  RunDocumentLifecycle();
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
}

TEST_F(HTMLElementTest, DirectStyleMutationCustomPropertyByAttributeStyleMap) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetBodyInnerHTML("<div id='box'></div>");
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  RunDocumentLifecycle();
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var box = document.getElementById('box');
    box.attributeStyleMap.set('--foo', '100px');
  )JS");
  GetDocument().body()->appendChild(script);
  EXPECT_TRUE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  RunDocumentLifecycle();
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
}

TEST_F(HTMLElementTest, DirectStyleMutationInFrame) {
  SetBodyInnerHTML(R"HTML(
    <iframe id='iframe'></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <div id='box'></div>
  )HTML");

  GetDocument().GetSettings()->SetScriptEnabled(true);
  ChildDocument().GetSettings()->SetScriptEnabled(true);
  EXPECT_FALSE(ChildDocument()
                   .GetPage()
                   ->Animator()
                   .has_inline_style_mutation_for_test());
  RunDocumentLifecycle();
  auto* script = ChildDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var box = document.getElementById('box');
    box.style.width = '100px';
    box.style.height = '100px';
  )JS");
  ChildDocument().body()->appendChild(script);
  EXPECT_TRUE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
}

TEST_F(HTMLElementTest, DirectStyleMutationNotTriggeredByInsertStyleSheet) {
  GetDocument().GetSettings()->SetScriptEnabled(true);

  SetBodyInnerHTML("<div id='box'></div>");
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var sheet = document.createElement('style');
    sheet.innerHTML = 'div { width:100px; }';
    document.body.appendChild(sheet);
  )JS");
  GetDocument().body()->appendChild(script);
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
}

TEST_F(HTMLElementTest, DirectStyleMutationNotTriggeredByToggleStyleChange) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    .mystyle {
      width: 100px;
    }
    </style>
    <div id='box'></div>
  )HTML");
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var box = document.getElementById('box');
    box.classList.toggle('mystyle');
  )JS");
  GetDocument().body()->appendChild(script);
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
}

TEST_F(HTMLElementTest,
       DirectStyleMutationNotTriggeredByPseudoClassStyleChange) {
  GetDocument().GetSettings()->SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
    .button {
      width: 50px;
      height: 50px;
    }
    .button:focus {
      width: 100px;
    }
    <button id='box' class='button'></button>
  )HTML");
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
  auto* script = GetDocument().CreateRawElement(html_names::kScriptTag);
  script->setTextContent(R"JS(
    var box = document.getElementById('box');
    box.focus();
  )JS");
  GetDocument().body()->appendChild(script);
  EXPECT_EQ(GetDocument().FocusedElement(),
            GetDocument().getElementById("box"));
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
}

TEST_F(HTMLElementTest, DirAutoByChildChanged) {
  ScopedCSSPseudoDirForTest scoped_feature(false);

  SetBodyInnerHTML("<div id='target' dir='auto'></div>");
  auto* element = GetDocument().getElementById("target");
  element->setTextContent(u"\u05D1");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(element->GetComputedStyle()->Direction(), TextDirection::kRtl);

  element->RemoveChildren();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(element->GetComputedStyle()->Direction(), TextDirection::kLtr);
}

TEST_F(HTMLElementTest, SlotDirAutoBySingleSlottedNodeRemoved) {
  ScopedCSSPseudoDirForTest scoped_feature(false);

  SetBodyInnerHTML("<div id='host'>slotted text</div>");
  auto* element = GetDocument().getElementById("host");
  ShadowRoot& shadow_root =
      element->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML(
      "<slot id='inner' dir='auto'><div>&#1571;</div></slot>");
  UpdateAllLifecyclePhasesForTest();

  Element* slot = shadow_root.getElementById("inner");
  EXPECT_EQ(slot->GetComputedStyle()->Direction(), TextDirection::kLtr);

  element->RemoveChildren();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(slot->GetComputedStyle()->Direction(), TextDirection::kRtl);
}

TEST_F(HTMLElementTest, HasAnchoredPopover) {
  ScopedHTMLPopoverAttributeForTest scoped_feature(true);

  SetBodyInnerHTML(R"HTML(
    <div id="anchor1"></div>
    <div id="anchor2"></div>
    <div id="target" popover anchor="anchor1"></div>
  )HTML");

  Element* anchor1 = GetDocument().getElementById("anchor1");
  Element* anchor2 = GetDocument().getElementById("anchor2");
  HTMLElement* target = To<HTMLElement>(GetDocument().getElementById("target"));

  EXPECT_EQ(target->anchorElement(), anchor1);
  EXPECT_TRUE(anchor1->HasAnchoredPopover());
  EXPECT_FALSE(anchor2->HasAnchoredPopover());

  target->setAttribute(html_names::kAnchorAttr, "anchor2");

  EXPECT_EQ(target->anchorElement(), anchor2);
  EXPECT_FALSE(anchor1->HasAnchoredPopover());
  EXPECT_TRUE(anchor2->HasAnchoredPopover());
}

TEST_F(HTMLElementTest, AnchoredPopoverIdChange) {
  ScopedHTMLPopoverAttributeForTest scoped_feature(true);

  SetBodyInnerHTML(R"HTML(
    <div id="anchor1"></div>
    <div id="anchor2"></div>
    <div id="target" popover anchor="anchor1"></div>
  )HTML");

  Element* anchor1 = GetDocument().getElementById("anchor1");
  Element* anchor2 = GetDocument().getElementById("anchor2");
  HTMLElement* target = To<HTMLElement>(GetDocument().getElementById("target"));

  EXPECT_EQ(target->anchorElement(), anchor1);
  EXPECT_TRUE(anchor1->HasAnchoredPopover());
  EXPECT_FALSE(anchor2->HasAnchoredPopover());

  anchor1->setAttribute(html_names::kIdAttr, "anchor2");
  anchor2->setAttribute(html_names::kIdAttr, "anchor1");

  EXPECT_EQ(target->anchorElement(), anchor2);
  EXPECT_FALSE(anchor1->HasAnchoredPopover());
  EXPECT_TRUE(anchor2->HasAnchoredPopover());
}

}  // namespace blink
