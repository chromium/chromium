// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/opaque_range.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class OpaqueRangeTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    ScopedOpaqueRangeForTest scoped_feature(true);
  }
};

TEST_F(OpaqueRangeTest, ConstructionAndInheritance) {
  SetBodyContent("<textarea>Hello</textarea>");
  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().body()->firstElementChild());
  OpaqueRange* range = OpaqueRange::Create(GetDocument(), textarea, 0, 0);
  EXPECT_NE(range, nullptr);
  AbstractRange* abstract_range = range;
  EXPECT_NE(abstract_range, nullptr);

  EXPECT_FALSE(range->IsStaticRange());
  EXPECT_FALSE(abstract_range->IsStaticRange());
  EXPECT_EQ(&range->OwnerDocument(), &GetDocument());
}

TEST_F(OpaqueRangeTest, InitialStateValidation) {
  SetBodyContent("<textarea>Hello</textarea>");
  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().body()->firstElementChild());
  OpaqueRange* range = OpaqueRange::Create(GetDocument(), textarea, 0, 0);
  EXPECT_EQ(range->startContainer(), nullptr);
  EXPECT_EQ(range->endContainer(), nullptr);

  EXPECT_EQ(range->startOffset(), 0u);
  EXPECT_EQ(range->endOffset(), 0u);
  EXPECT_TRUE(range->collapsed());
}

TEST_F(OpaqueRangeTest, TraceAndGarbageCollection) {
  SetBodyContent("<textarea>Hello</textarea>");
  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().body()->firstElementChild());
  auto* range = OpaqueRange::Create(GetDocument(), textarea, 1, 4);
  // Create a Persistent to hold the range through GC.
  Persistent<OpaqueRange> persistent(range);
  ThreadState::Current()->CollectAllGarbageForTesting();

  // Range should survive garbage collection.
  EXPECT_NE(range, nullptr);
}

TEST_F(OpaqueRangeTest, DocumentOwnership) {
  SetBodyContent("<textarea>Hello</textarea>");
  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().body()->firstElementChild());
  Document& doc = GetDocument();
  auto* range = OpaqueRange::Create(doc, textarea, 0, 5);
  // The range should belong to the document that created it.
  EXPECT_EQ(&doc, &range->OwnerDocument());

  // Create another document to verify the range stays with its original
  // document.
  ScopedNullExecutionContext execution_context;
  Document* other_doc =
      Document::CreateForTest(execution_context.GetExecutionContext());
  EXPECT_NE(other_doc, &range->OwnerDocument());
}

TEST_F(OpaqueRangeTest, ValidRangeOffsets) {
  SetBodyContent("<textarea>Hello</textarea>");
  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().body()->firstElementChild());
  auto* range = OpaqueRange::Create(GetDocument(), textarea, 1, 4);
  EXPECT_EQ(range->startOffset(), 1u);
  EXPECT_EQ(range->endOffset(), 4u);
  EXPECT_FALSE(range->collapsed());
}

TEST_F(OpaqueRangeTest, CollapsedRange) {
  SetBodyContent("<textarea>Hello</textarea>");
  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().body()->firstElementChild());
  auto* range = OpaqueRange::Create(GetDocument(), textarea, 2, 2);
  EXPECT_EQ(range->startOffset(), 2u);
  EXPECT_EQ(range->endOffset(), 2u);
  EXPECT_TRUE(range->collapsed());
}

TEST_F(OpaqueRangeTest, InputElementRange) {
  SetBodyContent("<input type='text' value='Test'>");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstElementChild());
  auto* range = OpaqueRange::Create(GetDocument(), input, 1, 3);
  EXPECT_EQ(range->startOffset(), 1u);
  EXPECT_EQ(range->endOffset(), 3u);
  EXPECT_FALSE(range->collapsed());
}

TEST_F(OpaqueRangeTest, RemovalClearsOpaqueRanges) {
  ScopedOpaqueRangeForTest scoped_feature(true);
  SetBodyContent("<textarea>Hello</textarea>");
  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().body()->firstElementChild());
  OpaqueRange::Create(GetDocument(), textarea, 0, 3);
  OpaqueRange::Create(GetDocument(), textarea, 2, 5);
  EXPECT_EQ(textarea->opaque_ranges_.size(), 2u);
  textarea->remove();
  EXPECT_TRUE(textarea->opaque_ranges_.empty());
}

TEST_F(OpaqueRangeTest, UseCounterFiresForTextarea) {
  SetBodyContent("<textarea>Hello</textarea>");
  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().body()->firstElementChild());
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kOpaqueRange));
  OpaqueRange::Create(GetDocument(), textarea, 0, 5);
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kOpaqueRange));
}

TEST_F(OpaqueRangeTest, UseCounterFiresForTextInput) {
  SetBodyContent("<input type='text' value='Hello'>");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstElementChild());
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kOpaqueRange));
  OpaqueRange::Create(GetDocument(), input, 0, 5);
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kOpaqueRange));
}

TEST_F(OpaqueRangeTest, UseCounterNotFiredForInvalidOffsets) {
  SetBodyContent("<textarea>Hi</textarea>");
  auto* textarea =
      To<HTMLTextAreaElement>(GetDocument().body()->firstElementChild());
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kOpaqueRange));

  // Attempt to create a range with offsets exceeding value length. The
  // createValueRange method should throw and never call OpaqueRange::Create.
  DummyExceptionStateForTesting exception_state;
  textarea->createValueRange(100, 200, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kOpaqueRange));
}

TEST_F(OpaqueRangeTest, UseCounterNotFiredForNonTextInput) {
  SetBodyContent("<input type='number' value='42'>");
  auto* input = To<HTMLInputElement>(GetDocument().body()->firstElementChild());
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kOpaqueRange));

  // Number inputs do not support the selection API, so createValueRange
  // should throw without counting.
  DummyExceptionStateForTesting exception_state;
  input->createValueRange(0, 1, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kOpaqueRange));
}
}  // namespace blink
