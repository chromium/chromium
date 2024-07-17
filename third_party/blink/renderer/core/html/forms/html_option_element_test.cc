// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/html/forms/html_option_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

template <typename T>
T* CreateElement(Document& document, const String& id) {
  T* element = MakeGarbageCollected<T>(document);
  element->SetIdAttribute(AtomicString(id));
  return element;
}

class HTMLOptionElementTest : public PageTestBase {
 protected:
  bool IsSelectAssociated(HTMLOptionElement* option) {
    // Option elements switch the structure of their UA shadowroot based on
    // whether they are associated with a <select> element. This checks the
    // shadowroot to see which mode it's in to make sure that the option element
    // is responding correctly to DOM mutations.
    return IsA<HTMLSlotElement>(option->GetShadowRoot()->firstChild());
  }

  VectorOf<HTMLOptionElement> OptionListToVector(HTMLSelectElement* select) {
    VectorOf<HTMLOptionElement> options;
    for (HTMLOptionElement* option : select->GetOptionList()) {
      options.push_back(option);
    }
    return options;
  }

  VectorOf<HTMLOptionElement> OptionCollectionToVector(
      HTMLSelectElement* select) {
    VectorOf<HTMLOptionElement> options;
    for (Element* option : *select->options()) {
      options.push_back(To<HTMLOptionElement>(option));
    }
    return options;
  }
};

TEST_F(HTMLOptionElementTest, DescendantOptionsInNestedSelects) {
  // <select id=parent_select>
  //   <datalist id=parents_datalist>
  //     <select id=child_select>
  //       <option id=child_option>
  //       <datalist id=datalist>
  //         <option id=datalist_child_option>
  //         <option id=datalist_child_option_2>
  //       <div id=child_div>
  //         <option id=child_div_option>
  auto* parent_select =
      CreateElement<HTMLSelectElement>(GetDocument(), "parent_select");
  GetDocument().body()->AppendChild(parent_select);
  auto* parents_datalist =
      CreateElement<HTMLDataListElement>(GetDocument(), "parents_datalist");
  parent_select->AppendChild(parents_datalist);
  auto* child_select =
      CreateElement<HTMLSelectElement>(GetDocument(), "child_select");
  parents_datalist->AppendChild(child_select);
  auto* child_option =
      CreateElement<HTMLOptionElement>(GetDocument(), "child_option");
  child_select->AppendChild(child_option);
  auto* datalist =
      CreateElement<HTMLDataListElement>(GetDocument(), "datalist");
  child_select->AppendChild(datalist);
  auto* datalist_child_option =
      CreateElement<HTMLOptionElement>(GetDocument(), "datalist_child_option");
  datalist->AppendChild(datalist_child_option);
  auto* datalist_child_option_2 = CreateElement<HTMLOptionElement>(
      GetDocument(), "datalist_child_option_2");
  datalist->AppendChild(datalist_child_option_2);
  auto* child_div = CreateElement<HTMLDivElement>(GetDocument(), "child_div");
  child_select->AppendChild(child_div);
  auto* child_div_option =
      CreateElement<HTMLOptionElement>(GetDocument(), "child_div_option");
  child_div->AppendChild(child_div_option);

  const VectorOf<HTMLOptionElement> empty;

  EXPECT_TRUE(child_option->WasOptionInsertedCalled());
  EXPECT_TRUE(datalist_child_option->WasOptionInsertedCalled());
  EXPECT_TRUE(datalist_child_option_2->WasOptionInsertedCalled());
  EXPECT_TRUE(child_div_option->WasOptionInsertedCalled());
  EXPECT_TRUE(IsSelectAssociated(child_option));
  EXPECT_TRUE(IsSelectAssociated(datalist_child_option));
  EXPECT_TRUE(IsSelectAssociated(datalist_child_option_2));
  EXPECT_TRUE(IsSelectAssociated(child_div_option));
  EXPECT_EQ(OptionListToVector(parent_select), empty);
  EXPECT_EQ(OptionCollectionToVector(parent_select), empty);
  VectorOf<HTMLOptionElement> expected1({child_option, datalist_child_option,
                                         datalist_child_option_2,
                                         child_div_option});
  EXPECT_EQ(OptionListToVector(child_select), expected1);
  EXPECT_EQ(OptionCollectionToVector(child_select), expected1);

  child_select->remove();
  parents_datalist->AppendChild(child_select);
  EXPECT_TRUE(child_option->WasOptionInsertedCalled());
  EXPECT_TRUE(datalist_child_option->WasOptionInsertedCalled());
  EXPECT_TRUE(datalist_child_option_2->WasOptionInsertedCalled());
  EXPECT_TRUE(child_div_option->WasOptionInsertedCalled());
  EXPECT_TRUE(IsSelectAssociated(child_option));
  EXPECT_TRUE(IsSelectAssociated(datalist_child_option));
  EXPECT_TRUE(IsSelectAssociated(datalist_child_option_2));
  EXPECT_TRUE(IsSelectAssociated(child_div_option));
  EXPECT_EQ(OptionListToVector(parent_select), empty);
  EXPECT_EQ(OptionCollectionToVector(parent_select), empty);
  EXPECT_EQ(OptionListToVector(child_select), expected1);
  EXPECT_EQ(OptionCollectionToVector(child_select), expected1);

  child_option->remove();
  EXPECT_FALSE(child_option->WasOptionInsertedCalled());
  EXPECT_FALSE(IsSelectAssociated(child_option));
  EXPECT_EQ(OptionListToVector(parent_select), empty);
  EXPECT_EQ(OptionCollectionToVector(parent_select), empty);
  VectorOf<HTMLOptionElement> expected3(
      {datalist_child_option, datalist_child_option_2, child_div_option});
  EXPECT_EQ(OptionListToVector(child_select), expected3);
  EXPECT_EQ(OptionCollectionToVector(child_select), expected3);

  datalist_child_option_2->remove();
  EXPECT_FALSE(datalist_child_option_2->WasOptionInsertedCalled());
  EXPECT_FALSE(IsSelectAssociated(datalist_child_option_2));
  EXPECT_EQ(OptionListToVector(parent_select), empty);
  EXPECT_EQ(OptionCollectionToVector(parent_select), empty);
  VectorOf<HTMLOptionElement> expected4(
      {datalist_child_option, child_div_option});
  EXPECT_EQ(OptionListToVector(child_select), expected4);
  EXPECT_EQ(OptionCollectionToVector(child_select), expected4);

  datalist->remove();
  EXPECT_FALSE(datalist_child_option->WasOptionInsertedCalled());
  EXPECT_FALSE(IsSelectAssociated(datalist_child_option));
  EXPECT_EQ(OptionListToVector(parent_select), empty);
  EXPECT_EQ(OptionCollectionToVector(parent_select), empty);
  VectorOf<HTMLOptionElement> expected5({child_div_option});
  EXPECT_EQ(OptionListToVector(child_select), expected5);
  EXPECT_EQ(OptionCollectionToVector(child_select), expected5);

  child_select->AppendChild(datalist);
  EXPECT_TRUE(datalist_child_option->WasOptionInsertedCalled());
  EXPECT_TRUE(IsSelectAssociated(datalist_child_option));
  EXPECT_EQ(OptionListToVector(parent_select), empty);
  EXPECT_EQ(OptionCollectionToVector(parent_select), empty);
  VectorOf<HTMLOptionElement> expected6(
      {child_div_option, datalist_child_option});
  EXPECT_EQ(OptionListToVector(child_select), expected6);
  EXPECT_EQ(OptionCollectionToVector(child_select), expected6);
}

}  // namespace blink
