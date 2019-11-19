// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class HTMLDataListElementTest : public SimTest {};

TEST_F(HTMLDataListElementTest, FinishedParsingChildren) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<datalist id=list></datalist>");

  auto* data_list = GetDocument().getElementById("list");
  ASSERT_TRUE(data_list);
  EXPECT_TRUE(data_list->IsFinishedParsingChildren());
}

}  // namespace blink
