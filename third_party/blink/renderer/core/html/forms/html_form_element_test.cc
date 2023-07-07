// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_form_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock-matchers.h"

namespace blink {

class HTMLFormElementTest : public PageTestBase {
 protected:
  void SetUp() override;
};

void HTMLFormElementTest::SetUp() {
  PageTestBase::SetUp();
  GetDocument().SetMimeType(AtomicString("text/html"));
}

TEST_F(HTMLFormElementTest, UniqueRendererFormId) {
  SetHtmlInnerHTML(
      "<body><form id='form1'></form><form id='form2'></form></body>");
  auto* form1 = To<HTMLFormElement>(GetElementById("form1"));
  uint64_t first_id = form1->UniqueRendererFormId();
  auto* form2 = To<HTMLFormElement>(GetElementById("form2"));
  EXPECT_EQ(first_id + 1, form2->UniqueRendererFormId());
  SetHtmlInnerHTML("<body><form id='form3'></form></body>");
  auto* form3 = To<HTMLFormElement>(GetElementById("form3"));
  EXPECT_EQ(first_id + 2, form3->UniqueRendererFormId());
}

// This tree is created manually because the HTML parser removes nested forms.
// The created tree looks like this:
// <body>
//   <form id=form1>
//     <form id=form2>
//       <input>
TEST_F(HTMLFormElementTest, ListedElementsNestedForms) {
  HTMLBodyElement* body = GetDocument().FirstBodyElement();

  HTMLFormElement* form1 = MakeGarbageCollected<HTMLFormElement>(GetDocument());
  body->AppendChild(form1);

  HTMLFormElement* form2 = MakeGarbageCollected<HTMLFormElement>(GetDocument());
  form1->AppendChild(form2);

  HTMLInputElement* input = MakeGarbageCollected<HTMLInputElement>(
      GetDocument(), CreateElementFlags::ByCreateElement());
  form2->AppendChild(input);

  ListedElement::List form1elements = form1->ListedElements();
  ListedElement::List form2elements = form2->ListedElements();
  EXPECT_EQ(form1elements.size(), 0u);
  ASSERT_EQ(form2elements.size(), 1u);
  EXPECT_EQ(form2elements.at(0)->ToHTMLElement(), input);
}

TEST_F(HTMLFormElementTest, ListedElementsDetachedForm) {
  HTMLBodyElement* body = GetDocument().FirstBodyElement();

  HTMLFormElement* form = MakeGarbageCollected<HTMLFormElement>(GetDocument());
  body->AppendChild(form);

  HTMLInputElement* input = MakeGarbageCollected<HTMLInputElement>(
      GetDocument(), CreateElementFlags::ByCreateElement());
  form->AppendChild(input);

  ListedElement::List listed_elements = form->ListedElements();
  ASSERT_EQ(listed_elements.size(), 1u);
  EXPECT_EQ(listed_elements.at(0)->ToHTMLElement(), input);

  form->remove();
  listed_elements = form->ListedElements();
  ASSERT_EQ(listed_elements.size(), 1u);
  EXPECT_EQ(listed_elements.at(0)->ToHTMLElement(), input);
}

// This tree is created manually because the HTML parser removes nested forms.
// The created tree looks like this:
// <body>
//   <form id=form1>
//     <div id=form1div>
//       <template shadowroot=open>
//         <form id=form2>
//           <form id=form3>
//             <div id=form3div>
//               <template shadowroot=open>
//
// An <input> element is appended at the bottom and moved up one node at a time
// in this tree, and each step of the way, ListedElements is checked on all
// forms.
TEST_F(HTMLFormElementTest, ListedElementsIncludeShadowTrees) {
  HTMLBodyElement* body = GetDocument().FirstBodyElement();

  HTMLFormElement* form1 = MakeGarbageCollected<HTMLFormElement>(GetDocument());
  body->AppendChild(form1);

  HTMLDivElement* form1div =
      MakeGarbageCollected<HTMLDivElement>(GetDocument());
  form1->AppendChild(form1div);
  ShadowRoot& form1root =
      form1div->AttachShadowRootInternal(ShadowRootType::kOpen);

  HTMLFormElement* form2 = MakeGarbageCollected<HTMLFormElement>(GetDocument());
  form1root.AppendChild(form2);

  HTMLFormElement* form3 = MakeGarbageCollected<HTMLFormElement>(GetDocument());
  form2->AppendChild(form3);

  HTMLDivElement* form3div =
      MakeGarbageCollected<HTMLDivElement>(GetDocument());
  form3->AppendChild(form3div);
  ShadowRoot& form3root =
      form3div->AttachShadowRootInternal(ShadowRootType::kOpen);

  HTMLInputElement* input = MakeGarbageCollected<HTMLInputElement>(
      GetDocument(), CreateElementFlags::ByCreateElement());

  form3root.AppendChild(input);
  EXPECT_EQ(form1->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form1->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{input});

  input->remove();
  EXPECT_EQ(form1->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form1->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});

  form3div->AppendChild(input);
  EXPECT_EQ(form1->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form1->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(), ListedElement::List{input});
  EXPECT_EQ(form3->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{input});

  form3->AppendChild(input);
  EXPECT_EQ(form1->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form1->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(), ListedElement::List{input});
  EXPECT_EQ(form3->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{input});

  input->remove();
  EXPECT_EQ(form1->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form1->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});

  form2->AppendChild(input);
  EXPECT_EQ(form1->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form1->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(), ListedElement::List{input});
  EXPECT_EQ(form2->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{input});
  EXPECT_EQ(form3->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});

  input->remove();
  EXPECT_EQ(form1->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form1->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});

  form1root.AppendChild(input);
  EXPECT_EQ(form1->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form1->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{input});
  EXPECT_EQ(form2->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});

  input->remove();
  EXPECT_EQ(form1->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form1->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});

  form1div->AppendChild(input);
  EXPECT_EQ(form1->ListedElements(), ListedElement::List{input});
  EXPECT_EQ(form1->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{input});
  EXPECT_EQ(form2->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});

  form1->AppendChild(input);
  EXPECT_EQ(form1->ListedElements(), ListedElement::List{input});
  EXPECT_EQ(form1->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{input});
  EXPECT_EQ(form2->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});

  input->remove();
  EXPECT_EQ(form1->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form1->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form2->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(), ListedElement::List{});
  EXPECT_EQ(form3->ListedElements(/*include_shadow_trees=*/true),
            ListedElement::List{});
}

// This tree is created manually because innerHTML assignment doesn't invoke the
// parser for declarative ShadowDOM. The created tree looks like this:
//  <form id=form1>
//    <input id=input1>
//    <div id=div1>
//      <template shadowroot=open>
//        <input id=input2>
//      </template>
//    </div>
//  </form>
TEST_F(HTMLFormElementTest, ListedElementsAfterIncludeShadowTrees) {
  HTMLBodyElement* body = GetDocument().FirstBodyElement();

  HTMLFormElement* form1 = MakeGarbageCollected<HTMLFormElement>(GetDocument());
  body->AppendChild(form1);

  HTMLInputElement* input1 = MakeGarbageCollected<HTMLInputElement>(
      GetDocument(), CreateElementFlags::ByCreateElement());
  form1->AppendChild(input1);

  HTMLDivElement* form1div =
      MakeGarbageCollected<HTMLDivElement>(GetDocument());
  form1->AppendChild(form1div);
  ShadowRoot& form1root =
      form1div->AttachShadowRootInternal(ShadowRootType::kOpen);

  HTMLInputElement* input2 = MakeGarbageCollected<HTMLInputElement>(
      GetDocument(), CreateElementFlags::ByCreateElement());
  form1root.AppendChild(input2);

  EXPECT_EQ(form1->ListedElements(), ListedElement::List{input1});
  ListedElement::List list;
  list.push_back(input1);
  list.push_back(input2);
  EXPECT_EQ(form1->ListedElements(/*include_shadow_trees=*/true), list);
  EXPECT_EQ(form1->ListedElements(), ListedElement::List{input1});
}

TEST_F(HTMLFormElementTest, ListedElementsIncludeShadowTreesFormAttribute) {
  HTMLBodyElement* body = GetDocument().FirstBodyElement();

  body->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <form id=form1>
      <div id=shadowhost>
        <template shadowroot=open>
          <input id=input2>
          <form id=form2>
            <input id=input3>
          </form>
          <input id=input4 form=form2>
        </template>
      </div>
    </form>
    <input id=input1 form=form1>
  )HTML");

  auto* form1 = To<HTMLFormElement>(GetElementById("form1"));
  auto* input1 = ListedElement::From(*GetElementById("input1"));
  auto* input2 =
      ListedElement::From(*GetElementById("shadowhost")
                               ->GetShadowRoot()
                               ->getElementById(AtomicString("input2")));

  EXPECT_THAT(form1->ListedElements(), ::testing::ElementsAre(input1));
  EXPECT_THAT(form1->ListedElements(/*include_shadow_trees=*/true),
              ::testing::ElementsAre(input2, input1));
}

}  // namespace blink
