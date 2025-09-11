// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/form_control_range.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
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

TEST_F(FormControlRangeTest, ConstructionAndInheritance) {
  FormControlRange* range = FormControlRange::Create(GetDocument());

  EXPECT_NE(range, nullptr);
  AbstractRange* abstract_range = range;
  EXPECT_NE(abstract_range, nullptr);

  EXPECT_FALSE(range->IsStaticRange());
  EXPECT_FALSE(abstract_range->IsStaticRange());
  EXPECT_EQ(&range->OwnerDocument(), &GetDocument());
}

TEST_F(FormControlRangeTest, InitialStateValidation) {
  FormControlRange* range = FormControlRange::Create(GetDocument());

  // Since form_control_ is null initially, containers will be null.
  EXPECT_EQ(range->startContainer(), nullptr);
  EXPECT_EQ(range->endContainer(), nullptr);

  EXPECT_EQ(range->startOffset(), 0u);
  EXPECT_EQ(range->endOffset(), 0u);
  EXPECT_TRUE(range->collapsed());
  EXPECT_EQ(range->toString(), g_empty_string);

  // Test null element validation.
  DummyExceptionStateForTesting exception_state;
  range->setFormControlRange(nullptr, 0, 0, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kNotSupportedError);
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

TEST_F(FormControlRangeTest, InvalidOffsetPreservesState) {
  SetBodyContent("<textarea>Hello</textarea>");
  auto* textarea = GetDocument().body()->firstElementChild();
  auto* range = FormControlRange::Create(GetDocument());

  // First set up a valid range.
  DummyExceptionStateForTesting valid_exception_state;
  range->setFormControlRange(textarea, 1, 4, valid_exception_state);
  EXPECT_FALSE(valid_exception_state.HadException());
  EXPECT_EQ(range->toString(), "ell");
  EXPECT_EQ(range->startOffset(), 1u);
  EXPECT_EQ(range->endOffset(), 4u);

  // Setting out-of-bounds offsets should fail and preserve previous state.
  DummyExceptionStateForTesting invalid_exception_state;
  range->setFormControlRange(textarea, 10, 15, invalid_exception_state);
  EXPECT_TRUE(invalid_exception_state.HadException());
  EXPECT_EQ(invalid_exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kIndexSizeError);
  EXPECT_EQ(range->toString(), "ell");
  EXPECT_EQ(range->startOffset(), 1u);
  EXPECT_EQ(range->endOffset(), 4u);
  EXPECT_EQ(range->startContainer(), textarea);
  EXPECT_EQ(range->endContainer(), textarea);
}

TEST_F(FormControlRangeTest, BackwardsRangesAutoCollapse) {
  SetBodyContent("<textarea>Hello</textarea>");
  auto* textarea = GetDocument().body()->firstElementChild();
  auto* range = FormControlRange::Create(GetDocument());

  // Backwards range should auto-collapse to [start, start].
  DummyExceptionStateForTesting exception_state;
  range->setFormControlRange(textarea, 4, 1, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_EQ(range->startOffset(), 4u);
  EXPECT_EQ(range->endOffset(), 4u);
  EXPECT_TRUE(range->collapsed());
  EXPECT_EQ(range->toString(), "");
}

class FormControlRangeElementTest
    : public FormControlRangeTest,
      public testing::WithParamInterface<std::tuple<const char*, bool>> {};

TEST_P(FormControlRangeElementTest, SetFormControlRangeElements) {
  const auto& [html, should_be_valid] = GetParam();
  SetBodyContent(html);
  auto* element = GetDocument().body()->firstElementChild();
  auto* range = FormControlRange::Create(GetDocument());

  DummyExceptionStateForTesting exception_state;
  range->setFormControlRange(element, 0, 4, exception_state);

  EXPECT_EQ(exception_state.HadException(), !should_be_valid);
  if (should_be_valid) {
    EXPECT_FALSE(exception_state.HadException());
    EXPECT_EQ(range->startContainer(), element);
    EXPECT_EQ(range->endContainer(), element);
    EXPECT_EQ(range->startOffset(), 0u);
    EXPECT_EQ(range->endOffset(), 4u);
    EXPECT_FALSE(range->collapsed());
    EXPECT_EQ(range->toString(), "Test");
  } else {
    EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kNotSupportedError);
  }
}

INSTANTIATE_TEST_SUITE_P(
    FormControlElements,
    FormControlRangeElementTest,
    testing::Values(
        std::make_tuple("<textarea>Test</textarea>", true),
        std::make_tuple("<input type='text' value='Test'>", true),
        std::make_tuple("<input type='search' value='Test'>", true),
        std::make_tuple("<input type='password' value='Test'>", true),
        std::make_tuple("<input type='url' value='Test'>", true),
        std::make_tuple("<input type='tel' value='Test'>", true),
        std::make_tuple("<div>Test</div>", false),
        std::make_tuple("<div contenteditable>Test</div>", false),
        std::make_tuple("<input type='checkbox'>", false),
        std::make_tuple("<input type='radio'>", false),
        std::make_tuple("<input type='submit'>", false),
        std::make_tuple("<input type='button'>", false),
        std::make_tuple("<select><option>Test</option></select>", false)));

struct OffsetTestCase {
  unsigned start_offset;
  unsigned end_offset;
  bool should_be_valid;
  std::optional<const char*> expected_text;
  std::optional<unsigned> expected_start;
  std::optional<unsigned> expected_end;
  std::optional<bool> expected_collapsed;
};

class FormControlRangeOffsetTest
    : public FormControlRangeTest,
      public testing::WithParamInterface<OffsetTestCase> {};

TEST_P(FormControlRangeOffsetTest, OffsetValidation) {
  SetBodyContent("<textarea>Hello</textarea>");
  auto* textarea = GetDocument().body()->firstElementChild();
  auto* range = FormControlRange::Create(GetDocument());

  DummyExceptionStateForTesting exception_state;
  range->setFormControlRange(textarea, GetParam().start_offset,
                             GetParam().end_offset, exception_state);

  EXPECT_EQ(exception_state.HadException(), !GetParam().should_be_valid);

  if (GetParam().should_be_valid) {
    EXPECT_FALSE(exception_state.HadException());
    if (GetParam().expected_text.has_value()) {
      EXPECT_EQ(range->toString(), GetParam().expected_text.value());
    }
    if (GetParam().expected_start.has_value()) {
      EXPECT_EQ(range->startOffset(), GetParam().expected_start.value());
    }
    if (GetParam().expected_end.has_value()) {
      EXPECT_EQ(range->endOffset(), GetParam().expected_end.value());
    }
    if (GetParam().expected_collapsed.has_value()) {
      EXPECT_EQ(range->collapsed(), GetParam().expected_collapsed.value());
    }
  } else {
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
              DOMExceptionCode::kIndexSizeError);
  }
}

INSTANTIATE_TEST_SUITE_P(
    OffsetCombinations,
    FormControlRangeOffsetTest,
    testing::Values(
        // Valid forward ranges with expected text.
        OffsetTestCase{0, 5, true, "Hello", 0, 5, false},
        OffsetTestCase{0, 0, true, "", 0, 0, true},
        OffsetTestCase{5, 5, true, "", 5, 5, true},
        OffsetTestCase{1, 4, true, "ell", 1, 4, false},
        OffsetTestCase{0, 1, true, "H", 0, 1, false},

        // Backwards ranges that should auto-collapse but not throw.
        OffsetTestCase{5, 2, true, "", 5, 5, true},
        OffsetTestCase{4, 1, true, "", 4, 4, true},
        OffsetTestCase{3, 0, true, "", 3, 3, true},

        // Invalid cases - out of bounds.
        OffsetTestCase{0, 10, false},   // end > value length.
        OffsetTestCase{10, 15, false},  // both offsets > value length.
        OffsetTestCase{6, 6, false},    // start at boundary but > value length.
        OffsetTestCase{3, 8, false},    // start valid but end > value length.
        OffsetTestCase{7, 10, false}    // both start and end > value length.
        ));

}  // namespace blink
