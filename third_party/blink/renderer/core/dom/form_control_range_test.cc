// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/form_control_range.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class FormControlRangeTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    ScopedFormControlRangeForTest scoped_feature(true);
  }
};

TEST_F(FormControlRangeTest, Constructible) {
  FormControlRange* range = FormControlRange::Create(GetDocument());

  EXPECT_NE(range, nullptr);
  AbstractRange* abstract_range = range;
  EXPECT_NE(abstract_range, nullptr);
}

TEST_F(FormControlRangeTest, InitialState) {
  FormControlRange* range = FormControlRange::Create(GetDocument());

  // Since form_control_ is null initially, containers will be null.
  EXPECT_EQ(range->startContainer(), nullptr);
  EXPECT_EQ(range->endContainer(), nullptr);

  EXPECT_EQ(range->startOffset(), 0u);
  EXPECT_EQ(range->endOffset(), 0u);
  EXPECT_TRUE(range->collapsed());
}

TEST_F(FormControlRangeTest, TraceAndGarbageCollection) {
  auto* range = FormControlRange::Create(GetDocument());

  // Create a Persistent to hold the range through GC.
  Persistent<FormControlRange> persistent(range);
  ThreadState::Current()->CollectAllGarbageForTesting();

  // Range should survive garbage collection.
  // Since form_control_ is null initially, containers will be null.
  EXPECT_EQ(range->startContainer(), nullptr);
  EXPECT_EQ(range->endContainer(), nullptr);
}

TEST_F(FormControlRangeTest, RangeType) {
  auto* range = FormControlRange::Create(GetDocument());
  AbstractRange* abstract_range = range;
  EXPECT_FALSE(abstract_range->IsStaticRange());
  EXPECT_FALSE(range->IsStaticRange());
}

TEST_F(FormControlRangeTest, DocumentOwnership) {
  Document& doc = GetDocument();
  auto* range = FormControlRange::Create(doc);

  // The range should belong to the document that created it.
  EXPECT_EQ(&doc, &range->OwnerDocument());

  // Create another document to verify the range stays with its original
  // document.
  ScopedNullExecutionContext execution_context;
  Document* other_doc =
      Document::CreateForTest(execution_context.GetExecutionContext());
  EXPECT_NE(other_doc, &range->OwnerDocument());
}

}  // namespace blink
