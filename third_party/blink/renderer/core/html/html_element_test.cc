// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
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
  GetDocument().getElementById(AtomicString("target"))->remove();
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
            GetDocument().getElementById(AtomicString("box")));
  EXPECT_FALSE(
      GetDocument().GetPage()->Animator().has_inline_style_mutation_for_test());
}

TEST_F(HTMLElementTest, MayBeImplicitAnchor) {
  SetBodyInnerHTML(R"HTML(
    <div id="anchor1"></div>
    <div id="anchor2"></div>
    <div id="target" anchor="anchor1"></div>
  )HTML");

  Element* anchor1 = GetDocument().getElementById(AtomicString("anchor1"));
  Element* anchor2 = GetDocument().getElementById(AtomicString("anchor2"));
  HTMLElement* target =
      To<HTMLElement>(GetDocument().getElementById(AtomicString("target")));

  EXPECT_EQ(target->anchorElement(), anchor1);
  EXPECT_TRUE(anchor1->MayBeImplicitAnchor());
  EXPECT_FALSE(anchor2->MayBeImplicitAnchor());

  target->setAttribute(html_names::kAnchorAttr, AtomicString("anchor2"));

  EXPECT_EQ(target->anchorElement(), anchor2);
  EXPECT_TRUE(anchor1->MayBeImplicitAnchor());
  EXPECT_TRUE(anchor2->MayBeImplicitAnchor());

  target->removeAttribute(html_names::kAnchorAttr);

  EXPECT_FALSE(target->anchorElement());
  EXPECT_TRUE(anchor1->MayBeImplicitAnchor());
  EXPECT_TRUE(anchor2->MayBeImplicitAnchor());
}

TEST_F(HTMLElementTest, MayBeImplicitAnchorViaElementAttr) {
  SetBodyInnerHTML(R"HTML(
    <div id="anchor1"></div>
    <div id="anchor2"></div>
    <div id="target" anchor="anchor1"></div>
  )HTML");

  Element* anchor1 = GetDocument().getElementById(AtomicString("anchor1"));
  Element* anchor2 = GetDocument().getElementById(AtomicString("anchor2"));
  HTMLElement* target =
      To<HTMLElement>(GetDocument().getElementById(AtomicString("target")));

  EXPECT_EQ(target->anchorElement(), anchor1);
  EXPECT_TRUE(anchor1->MayBeImplicitAnchor());
  EXPECT_FALSE(anchor2->MayBeImplicitAnchor());

  target->setAnchorElementForBinding(anchor2);

  EXPECT_EQ(target->anchorElement(), anchor2);
  EXPECT_TRUE(anchor1->MayBeImplicitAnchor());
  EXPECT_TRUE(anchor2->MayBeImplicitAnchor());

  target->setAnchorElementForBinding(nullptr);

  EXPECT_FALSE(target->anchorElement());
  EXPECT_TRUE(anchor1->MayBeImplicitAnchor());
  EXPECT_TRUE(anchor2->MayBeImplicitAnchor());

  target->setAttribute(html_names::kAnchorAttr, AtomicString("anchor1"));

  EXPECT_EQ(target->anchorElement(), anchor1);
  EXPECT_TRUE(anchor1->MayBeImplicitAnchor());
  EXPECT_TRUE(anchor2->MayBeImplicitAnchor());
}

TEST_F(HTMLElementTest, ImplicitAnchorIdChange) {
  SetBodyInnerHTML(R"HTML(
    <div id="anchor1"></div>
    <div id="anchor2"></div>
    <div id="target" anchor="anchor1"></div>
  )HTML");

  Element* anchor1 = GetDocument().getElementById(AtomicString("anchor1"));
  Element* anchor2 = GetDocument().getElementById(AtomicString("anchor2"));
  HTMLElement* target =
      To<HTMLElement>(GetDocument().getElementById(AtomicString("target")));

  EXPECT_EQ(target->anchorElement(), anchor1);
  EXPECT_TRUE(anchor1->MayBeImplicitAnchor());
  EXPECT_FALSE(anchor2->MayBeImplicitAnchor());

  anchor1->setAttribute(html_names::kIdAttr, AtomicString("anchor2"));
  anchor2->setAttribute(html_names::kIdAttr, AtomicString("anchor1"));

  EXPECT_EQ(target->anchorElement(), anchor2);
  EXPECT_TRUE(anchor1->MayBeImplicitAnchor());
  EXPECT_TRUE(anchor2->MayBeImplicitAnchor());
}

TEST_F(HTMLElementTest, ImplicitlyAnchorElementConnected) {
  SetBodyInnerHTML("<div id=anchor></div>");

  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));

  HTMLElement* target1 = To<HTMLElement>(
      GetDocument().CreateElementForBinding(AtomicString("div")));
  target1->setAttribute(html_names::kAnchorAttr, AtomicString("anchor"));

  HTMLElement* target2 = To<HTMLElement>(
      GetDocument().CreateElementForBinding(AtomicString("div")));
  target2->setAnchorElementForBinding(anchor);

  EXPECT_FALSE(target1->anchorElement());
  EXPECT_FALSE(target2->anchorElement());
  EXPECT_FALSE(anchor->MayBeImplicitAnchor());

  GetDocument().body()->appendChild(target1);
  GetDocument().body()->appendChild(target2);

  EXPECT_EQ(target1->anchorElement(), anchor);
  EXPECT_EQ(target2->anchorElement(), anchor);
  EXPECT_TRUE(anchor->MayBeImplicitAnchor());
}

TEST_F(HTMLElementTest, PopoverTopLayerRemovalTiming) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" popover></div>
  )HTML");

  HTMLElement* target =
      To<HTMLElement>(GetDocument().getElementById(AtomicString("target")));

  EXPECT_FALSE(target->popoverOpen());
  EXPECT_FALSE(target->IsInTopLayer());
  target->ShowPopoverInternal(/*invoker*/ nullptr, /*exception_state*/ nullptr);
  EXPECT_TRUE(target->popoverOpen());
  EXPECT_TRUE(target->IsInTopLayer());

  // HidePopoverInternal causes :closed to match immediately, but schedules
  // the removal from the top layer.
  target->HidePopoverInternal(
      /*invoker=*/nullptr, HidePopoverFocusBehavior::kFocusPreviousElement,
      HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
      /*exception_state*/ nullptr);
  EXPECT_FALSE(target->popoverOpen());
  EXPECT_TRUE(target->IsInTopLayer());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->IsInTopLayer());

  // Document removal should cause immediate top layer removal.
  target->ShowPopoverInternal(/*invoker*/ nullptr, /*exception_state*/ nullptr);
  EXPECT_TRUE(target->popoverOpen());
  EXPECT_TRUE(target->IsInTopLayer());
  target->remove();
  EXPECT_FALSE(target->popoverOpen());
  EXPECT_FALSE(target->IsInTopLayer());
}

TEST_F(HTMLElementTest, DialogTopLayerRemovalTiming) {
  SetBodyInnerHTML(R"HTML(
    <dialog id="target"></dialog>
  )HTML");

  auto* target = To<HTMLDialogElement>(
      GetDocument().getElementById(AtomicString("target")));

  EXPECT_FALSE(target->IsInTopLayer());
  target->showModal(ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(target->IsInTopLayer());
  target->close();
  EXPECT_TRUE(target->IsInTopLayer());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(target->IsInTopLayer());
}

TEST_F(HTMLElementTest, InertAttributeUseCounted) {
  SetBodyInnerHTML(R"HTML(
    <div inert></div>
  )HTML");
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kInertAttribute));
  GetDocument().ClearUseCounterForTesting(WebFeature::kInertAttribute);

  // Set via setAttribute
  SetBodyInnerHTML(R"HTML(
    <div id=target></div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kInertAttribute));
  GetDocument()
      .getElementById(AtomicString("target"))
      ->setAttribute(html_names::kInertAttr, AtomicString("true"));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kInertAttribute));
  GetDocument().ClearUseCounterForTesting(WebFeature::kInertAttribute);

  // Test that the use counter is not incremented when the inert is
  // set via style.
  SetBodyInnerHTML(R"HTML(
    <div style="interactivity: inert;"></div>
  )HTML");
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kInertAttribute));
  GetDocument().ClearUseCounterForTesting(WebFeature::kInertAttribute);
}

TEST_F(HTMLElementTest, TitleAttributeDirectionality) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #text { height: 20px; }
      #target { height: 100px; }
    </style>
    <div id="parent">
      <div id="container">
        <div id="text"></div>
        <div id="target"></div>
      </div>
    </div>
  )HTML");

  Element* parent = GetElementById("parent");
  Element* container = GetElementById("container");
  HTMLElement* text = DynamicTo<HTMLElement>(GetElementById("text"));
  Element* target = GetElementById("target");

  AtomicString a_b_c("abc");
  AtomicString aleph_beth_gimel(u"\u05D0\u05D1\u05D2");
  AtomicString kLtr("ltr");
  AtomicString kRtl("rtl");
  AtomicString kAuto("auto");

  auto get_title_direction = [this, target]() -> TextDirection {
    const HitTestRequest hit_request(HitTestRequest::kActive);
    const HitTestLocation hit_location(PhysicalOffset(50, 70));
    HitTestResult hit_result(hit_request, hit_location);
    EXPECT_TRUE(GetLayoutView().HitTest(hit_location, hit_result));
    EXPECT_EQ(hit_result.InnerNode(), target);
    TextDirection dir;
    EXPECT_FALSE(hit_result.Title(dir).IsNull());
    return dir;
  };

  text->setInnerText(aleph_beth_gimel);
  container->setAttribute(html_names::kTitleAttr, a_b_c);
  EXPECT_EQ(get_title_direction(), TextDirection::kLtr);
  container->setAttribute(html_names::kDirAttr, kRtl);
  EXPECT_EQ(get_title_direction(), TextDirection::kRtl);
  container->setAttribute(html_names::kDirAttr, kLtr);
  EXPECT_EQ(get_title_direction(), TextDirection::kLtr);
  container->setAttribute(html_names::kDirAttr,
                          kAuto);  // RTL for contents, LTR for attribute
  EXPECT_EQ(get_title_direction(), TextDirection::kLtr);
  container->removeAttribute(html_names::kDirAttr);
  parent->setAttribute(html_names::kDirAttr, kAuto);  // RTL
  EXPECT_EQ(get_title_direction(), TextDirection::kRtl);
  text->setInnerText(a_b_c);  // now auto is LTR
  container->setAttribute(html_names::kTitleAttr, aleph_beth_gimel);
  EXPECT_EQ(get_title_direction(), TextDirection::kLtr);
  parent->removeAttribute(html_names::kDirAttr);
  container->setAttribute(html_names::kDirAttr,
                          kAuto);  // LTR for contents, RTL for attribute
  EXPECT_EQ(get_title_direction(), TextDirection::kRtl);
}

}  // namespace blink
