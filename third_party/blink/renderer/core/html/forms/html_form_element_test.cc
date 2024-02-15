// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_form_element.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock-matchers.h"

namespace blink {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

class HTMLFormElementTest : public PageTestBase {
 protected:
  void SetUp() override;

  HTMLFormElement* GetFormElement(const char* id,
                                  ShadowRoot* shadow_root = nullptr) {
    return DynamicTo<HTMLFormElement>(
        shadow_root ? shadow_root->getElementById(AtomicString(id))
                    : GetElementById(id));
  }

  ListedElement* GetListedElement(const char* id,
                                  ShadowRoot* shadow_root = nullptr) {
    if (Element* element = shadow_root
                               ? shadow_root->getElementById(AtomicString(id))
                               : GetElementById(id)) {
      return ListedElement::From(*element);
    }
    return nullptr;
  }
};

void HTMLFormElementTest::SetUp() {
  PageTestBase::SetUp();
  GetDocument().SetMimeType(AtomicString("text/html"));
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

  HTMLInputElement* input =
      MakeGarbageCollected<HTMLInputElement>(GetDocument());
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

  HTMLInputElement* input =
      MakeGarbageCollected<HTMLInputElement>(GetDocument());
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
//       <template shadowrootmode=open>
//         <form id=form2>
//           <form id=form3>
//             <div id=form3div>
//               <template shadowrootmode=open>
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
      form1div->AttachShadowRootForTesting(ShadowRootType::kOpen);

  HTMLFormElement* form2 = MakeGarbageCollected<HTMLFormElement>(GetDocument());
  form1root.AppendChild(form2);

  HTMLFormElement* form3 = MakeGarbageCollected<HTMLFormElement>(GetDocument());
  form2->AppendChild(form3);

  HTMLDivElement* form3div =
      MakeGarbageCollected<HTMLDivElement>(GetDocument());
  form3->AppendChild(form3div);
  ShadowRoot& form3root =
      form3div->AttachShadowRootForTesting(ShadowRootType::kOpen);

  HTMLInputElement* input =
      MakeGarbageCollected<HTMLInputElement>(GetDocument());

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

TEST_F(HTMLFormElementTest, ListedElementsAfterIncludeShadowTrees) {
  HTMLBodyElement* body = GetDocument().FirstBodyElement();
  body->setHTMLUnsafe(R"HTML(
    <form id=form1>
      <input id=input1>
      <div id=div1>
        <template shadowrootmode=open>
          <input id=input2>
        </template>
      </div>
    </form>
  )HTML");

  HTMLFormElement* form1 = GetFormElement("form1");
  ASSERT_NE(form1, nullptr);
  EXPECT_THAT(
      form1->ListedElements(/*include_shadow_trees=*/true),
      ElementsAre(
          GetListedElement("input1"),
          GetListedElement("input2", GetElementById("div1")->GetShadowRoot())));
  EXPECT_THAT(form1->ListedElements(), ElementsAre(GetListedElement("input1")));
}

TEST_F(HTMLFormElementTest, ListedElementsIncludeShadowTreesFormAttribute) {
  HTMLBodyElement* body = GetDocument().FirstBodyElement();

  body->setHTMLUnsafe(R"HTML(
    <form id=form1>
      <div id=shadowhost>
        <template shadowrootmode=open>
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

  HTMLFormElement* form1 = GetFormElement("form1");
  EXPECT_THAT(form1->ListedElements(), ElementsAre(GetListedElement("input1")));
  EXPECT_THAT(
      form1->ListedElements(/*include_shadow_trees=*/true),
      ElementsAre(GetListedElement(
                      "input2", GetElementById("shadowhost")->GetShadowRoot()),
                  GetListedElement("input1")));
}

// Tests that form control elements inside nested forms are extracted if
// `kAutofillIncludeFormElementsInShadowDom` is enabled.
TEST_F(HTMLFormElementTest, ListedElementsInNestedForms) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillIncludeFormElementsInShadowDom};

  HTMLBodyElement* body = GetDocument().FirstBodyElement();
  body->setHTMLUnsafe(R"HTML(
    <form id=f1>
      <div id=shadowhost>
        <template shadowrootmode=open>
          <input id=i1>
          <form id=f2>
            <input id=i2>
          </form>
          <input id=i3 form=f2>
        </template>
      </div>
    </form>
  )HTML");

  ShadowRoot* shadow_root = GetElementById("shadowhost")->GetShadowRoot();
  ASSERT_NE(shadow_root, nullptr);
  HTMLFormElement* f1 = GetFormElement("f1");
  ASSERT_NE(f1, nullptr);

  EXPECT_THAT(f1->ListedElements(), IsEmpty());
  EXPECT_THAT(f1->ListedElements(/*include_shadow_trees=*/true),
              ElementsAre(GetListedElement("i1", shadow_root),
                          GetListedElement("i2", shadow_root),
                          GetListedElement("i3", shadow_root)));
}

}  // namespace blink
