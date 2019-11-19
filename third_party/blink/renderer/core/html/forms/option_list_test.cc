// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/option_list.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

AtomicString Id(const HTMLOptionElement* option) {
  return option->FastGetAttribute(html_names::kIdAttr);
}

}  // namespace

class OptionListTest : public testing::Test {
 protected:
  void SetUp() override {
    auto* document = MakeGarbageCollected<HTMLDocument>();
    auto* select = MakeGarbageCollected<HTMLSelectElement>(*document);
    document->AppendChild(select);
    select_ = select;
  }
  HTMLSelectElement& Select() const { return *select_; }

 private:
  Persistent<HTMLSelectElement> select_;
};

TEST_F(OptionListTest, Empty) {
  OptionList list = Select().GetOptionList();
  EXPECT_EQ(list.end(), list.begin())
      << "OptionList should iterate over empty SELECT successfully";
}

TEST_F(OptionListTest, OptionOnly) {
  Select().SetInnerHTMLFromString(
      "text<input><option id=o1></option><input><option "
      "id=o2></option><input>");
  auto* div = To<HTMLElement>(
      Select().GetDocument().CreateRawElement(html_names::kDivTag));
  div->SetInnerHTMLFromString("<option id=o3></option>");
  Select().AppendChild(div);
  OptionList list = Select().GetOptionList();
  OptionList::Iterator iter = list.begin();
  EXPECT_EQ("o1", Id(*iter));
  ++iter;
  EXPECT_EQ("o2", Id(*iter));
  ++iter;
  // No "o3" because it's in DIV.
  EXPECT_EQ(list.end(), iter);
}

TEST_F(OptionListTest, Optgroup) {
  Select().SetInnerHTMLFromString(
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
      ->SetInnerHTMLFromString(
          "<optgroup><option id=gg11></option></optgroup>"
          "<option id=g11></option>");
  list = Select().GetOptionList();
  iter = list.begin();
  EXPECT_EQ("g11", Id(*iter)) << "Nested OPTGROUP should be ignored.";
}

}  // naemespace blink
