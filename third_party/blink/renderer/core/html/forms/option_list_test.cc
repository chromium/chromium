// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/option_list.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

AtomicString Id(const HTMLOptionElement* option) {
  return option->FastGetAttribute(html_names::kIdAttr);
}

}  // namespace

class OptionListTest : public testing::Test {
 protected:
  void SetUp() override {
    auto* document =
        HTMLDocument::CreateForTest(execution_context_.GetExecutionContext());
    auto* select = MakeGarbageCollected<HTMLSelectElement>(*document);
    document->AppendChild(select);
    select_ = select;
  }
  HTMLSelectElement& Select() const { return *select_; }

 private:
  test::TaskEnvironment task_environment_;
  ScopedNullExecutionContext execution_context_;
  Persistent<HTMLSelectElement> select_;
};

TEST_F(OptionListTest, Empty) {
  OptionList list = Select().GetOptionList();
  EXPECT_EQ(list.end(), list.begin())
      << "OptionList should iterate over empty SELECT successfully";
}

TEST_F(OptionListTest, OptionOnly) {
  Select().setInnerHTML(
      "text<input><option id=o1></option><input><option "
      "id=o2></option><input>");
  auto* div = To<HTMLElement>(
      Select().GetDocument().CreateRawElement(html_names::kDivTag));
  div->setInnerHTML("<option id=o3></option>");
  Select().AppendChild(div);
  OptionList list = Select().GetOptionList();
  OptionList::Iterator iter = list.begin();
  EXPECT_EQ("o1", Id(*iter));
  ++iter;
  EXPECT_EQ("o2", Id(*iter));
  ++iter;
  // Include "o3" even though it's in a DIV.
  EXPECT_EQ("o3", Id(*iter));
  ++iter;
  EXPECT_EQ(list.end(), iter);
}

TEST_F(OptionListTest, Optgroup) {
  Select().setInnerHTML(
      "<optgroup><option id=g11></option><option id=g12></option></optgroup>"
      "<optgroup><option id=g21></option></optgroup>"
      "<optgroup></optgroup>"
      "<option id=o1></option>"
      "<optgroup><option id=g41></option></optgroup>");
  OptionList list = Select().GetOptionList();
  OptionList::Iterator iter = list.begin();
  EXPECT_EQ("g11", Id(*iter));
  ++iter;
  EXPECT_EQ("g12", Id(*iter));
  ++iter;
  EXPECT_EQ("g21", Id(*iter));
  ++iter;
  EXPECT_EQ("o1", Id(*iter));
  ++iter;
  EXPECT_EQ("g41", Id(*iter));
  ++iter;
  EXPECT_EQ(list.end(), iter);

  To<HTMLElement>(Select().firstChild())
      ->setInnerHTML(
          "<optgroup><option id=gg11></option></optgroup>"
          "<option id=g11></option>");
  list = Select().GetOptionList();
  iter = list.begin();
  EXPECT_EQ("gg11", Id(*iter)) << "Nested OPTGROUP should be included.";
}

}  // naemespace blink
