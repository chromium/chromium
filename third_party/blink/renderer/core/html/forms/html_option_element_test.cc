// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_option_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class HTMLOptionElementTest : public PageTestBase {
 protected:
  bool GetSelectDescendantFlag(HTMLOptionElement* option) {
    return option->is_descendant_of_select_list_or_select_datalist_;
  }

  void ExpectSelectOptions(HTMLSelectElement* select,
                           const VectorOf<HTMLOptionElement>& options) {
    auto option_list_iterator = select->GetOptionList().begin();
    auto option_collection_iterator = select->options()->begin();
    auto expected_list_iterator = options.begin();
    while (option_list_iterator != select->GetOptionList().end() ||
           expected_list_iterator != options.end() ||
           option_collection_iterator != select->options()->end()) {
      bool expected_at_end = expected_list_iterator == options.end();
      bool option_list_at_end =
          option_list_iterator == select->GetOptionList().end();
      bool option_collection_at_end =
          !(option_collection_iterator != select->options()->end());
      ASSERT_EQ(option_list_at_end, expected_at_end);
      ASSERT_EQ(option_collection_at_end, expected_at_end);
      ASSERT_EQ(*option_list_iterator, *expected_list_iterator);
      ASSERT_EQ(*option_collection_iterator, *expected_list_iterator);
      ++option_list_iterator;
      ++expected_list_iterator;
      ++option_collection_iterator;
    }
  }
};

TEST_F(HTMLOptionElementTest, DescendantOptionsInNestedSelects) {
  // The HTML parser won't allow us to add nested <select>s, but here is the
  // structure:
  // <select id=parent_select>
  //   <datalist id=parents_datalist>
  //     <select id=child_select>
  //       <option id=child_option>
  //       <datalist id=datalist>
  //         <option id=datalist_child_option>
  //         <option id=datalist_child_option_2>
  //       <div id=child_div>
  //         <option id=child_div_option>
  auto* parent_select = MakeGarbageCollected<HTMLSelectElement>(GetDocument());
  GetDocument().body()->AppendChild(parent_select);
  auto* parents_datalist =
      MakeGarbageCollected<HTMLDataListElement>(GetDocument());
  parent_select->AppendChild(parents_datalist);
  auto* child_select = MakeGarbageCollected<HTMLSelectElement>(GetDocument());
  parents_datalist->AppendChild(child_select);
  auto* child_option = MakeGarbageCollected<HTMLOptionElement>(GetDocument());
  child_select->AppendChild(child_option);
  auto* datalist = MakeGarbageCollected<HTMLDataListElement>(GetDocument());
  child_select->AppendChild(datalist);
  auto* datalist_child_option =
      MakeGarbageCollected<HTMLOptionElement>(GetDocument());
  datalist->AppendChild(datalist_child_option);
  auto* datalist_child_option_2 =
      MakeGarbageCollected<HTMLOptionElement>(GetDocument());
  datalist->AppendChild(datalist_child_option_2);
  auto* child_div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  child_select->AppendChild(child_div);
  auto* child_div_option =
      MakeGarbageCollected<HTMLOptionElement>(GetDocument());
  child_div->AppendChild(child_div_option);

  EXPECT_TRUE(child_option->WasOptionInsertedCalled());
  EXPECT_TRUE(datalist_child_option->WasOptionInsertedCalled());
  EXPECT_TRUE(datalist_child_option_2->WasOptionInsertedCalled());
  EXPECT_FALSE(child_div_option->WasOptionInsertedCalled());
  EXPECT_FALSE(GetSelectDescendantFlag(child_option));
  EXPECT_TRUE(GetSelectDescendantFlag(datalist_child_option));
  EXPECT_TRUE(GetSelectDescendantFlag(datalist_child_option_2));
  EXPECT_FALSE(GetSelectDescendantFlag(child_div_option));
  ExpectSelectOptions(parent_select, {});
  ExpectSelectOptions(child_select, {child_option, datalist_child_option,
                                     datalist_child_option_2});

  child_select->remove();
  parents_datalist->AppendChild(child_select);
  EXPECT_TRUE(child_option->WasOptionInsertedCalled());
  EXPECT_TRUE(datalist_child_option->WasOptionInsertedCalled());
  EXPECT_TRUE(datalist_child_option_2->WasOptionInsertedCalled());
  EXPECT_FALSE(child_div_option->WasOptionInsertedCalled());
  EXPECT_FALSE(GetSelectDescendantFlag(child_option));
  EXPECT_TRUE(GetSelectDescendantFlag(datalist_child_option));
  EXPECT_TRUE(GetSelectDescendantFlag(datalist_child_option_2));
  EXPECT_FALSE(GetSelectDescendantFlag(child_div_option));
  ExpectSelectOptions(parent_select, {});
  ExpectSelectOptions(child_select, {child_option, datalist_child_option,
                                     datalist_child_option_2});

  child_option->remove();
  EXPECT_FALSE(child_option->WasOptionInsertedCalled());
  EXPECT_FALSE(GetSelectDescendantFlag(child_option));
  ExpectSelectOptions(parent_select, {});
  ExpectSelectOptions(child_select,
                      {datalist_child_option, datalist_child_option_2});

  datalist_child_option_2->remove();
  EXPECT_FALSE(datalist_child_option_2->WasOptionInsertedCalled());
  EXPECT_FALSE(GetSelectDescendantFlag(datalist_child_option_2));
  ExpectSelectOptions(parent_select, {});
  ExpectSelectOptions(child_select, {datalist_child_option});

  datalist->remove();
  EXPECT_FALSE(datalist_child_option->WasOptionInsertedCalled());
  EXPECT_FALSE(GetSelectDescendantFlag(datalist_child_option));
  ExpectSelectOptions(parent_select, {});
  ExpectSelectOptions(child_select, {});

  child_select->AppendChild(datalist);
  EXPECT_TRUE(datalist_child_option->WasOptionInsertedCalled());
  EXPECT_TRUE(GetSelectDescendantFlag(datalist_child_option));
  ExpectSelectOptions(parent_select, {});
  ExpectSelectOptions(child_select, {datalist_child_option});
}

}  // namespace blink
