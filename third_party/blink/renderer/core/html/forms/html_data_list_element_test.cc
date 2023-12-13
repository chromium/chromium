// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class HTMLDataListElementTest : public SimTest {};

TEST_F(HTMLDataListElementTest, FinishedParsingChildren) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");
  main_resource.Complete("<datalist id=list></datalist>");

  auto* data_list = GetDocument().getElementById(AtomicString("list"));
  ASSERT_TRUE(data_list);
  EXPECT_TRUE(data_list->IsFinishedParsingChildren());
}

TEST(HTMLDataListElementTest2, DecrementedAfterGc) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  Persistent<Document> document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body><datalist id=x></datalist></body>");
  EXPECT_TRUE(document->HasAtLeastOneDataList());
  auto* data_list = document->getElementById(AtomicString("x"));
  ASSERT_TRUE(data_list);
  data_list->parentElement()->RemoveChild(data_list);
  data_list = nullptr;
  blink::ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(document->HasAtLeastOneDataList());
}

}  // namespace blink
