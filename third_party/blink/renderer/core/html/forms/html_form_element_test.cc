// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_form_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class HTMLFormElementTest : public PageTestBase {
 protected:
  void SetUp() override;
};

void HTMLFormElementTest::SetUp() {
  PageTestBase::SetUp();
  GetDocument().SetMimeType("text/html");
}

TEST_F(HTMLFormElementTest, UniqueRendererFormId) {
  SetHtmlInnerHTML(
      "<body><form id='form1'></form><form id='form2'></form></body>");
  auto* form1 = To<HTMLFormElement>(GetElementById("form1"));
  unsigned first_id = form1->UniqueRendererFormId();
  auto* form2 = To<HTMLFormElement>(GetElementById("form2"));
  EXPECT_EQ(first_id + 1, form2->UniqueRendererFormId());
  SetHtmlInnerHTML("<body><form id='form3'></form></body>");
  auto* form3 = To<HTMLFormElement>(GetElementById("form3"));
  EXPECT_EQ(first_id + 2, form3->UniqueRendererFormId());
}

}  // namespace blink
