// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BASE_LAYOUT_ALGORITHM_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BASE_LAYOUT_ALGORITHM_TEST_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Element;
class LayoutNGBlockFlow;
class NGBlockNode;
class NGBreakToken;
class NGPhysicalBoxFragment;

// Base class for all LayoutNG Algorithms unit test classes.
typedef bool TestParamLayoutNG;
class NGBaseLayoutAlgorithmTest
    : public testing::WithParamInterface<TestParamLayoutNG>,
      public RenderingTest {
 protected:
  void SetUp() override;

  // Should be called before calling Layout(), if you're not using
  // RunBlockLayoutAlgorithmForElement.
  void AdvanceToLayoutPhase();

  const NGPhysicalBoxFragment* RunBlockLayoutAlgorithm(
      NGBlockNode node,
      const NGConstraintSpace& space,
      const NGBreakToken* break_token = nullptr);

  std::pair<const NGPhysicalBoxFragment*, NGConstraintSpace>
  RunBlockLayoutAlgorithmForElement(Element* element);

  const NGPhysicalBoxFragment* RunFieldsetLayoutAlgorithm(
      NGBlockNode node,
      const NGConstraintSpace& space,
      const NGBreakToken* break_token = nullptr);

  const NGPhysicalBoxFragment* GetBoxFragmentByElementId(const char*);

  static const NGPhysicalBoxFragment* CurrentFragmentFor(
      const LayoutNGBlockFlow*);
};

class FragmentChildIterator {
  STACK_ALLOCATED();

 public:
  explicit FragmentChildIterator(const NGPhysicalBoxFragment* parent) {
    SetParent(parent);
  }
  void SetParent(const NGPhysicalBoxFragment* parent) {
    parent_ = parent;
    index_ = 0;
  }

  const NGPhysicalBoxFragment* NextChild(
      PhysicalOffset* fragment_offset = nullptr);

 private:
  const NGPhysicalBoxFragment* parent_;
  unsigned index_;
};

NGConstraintSpace ConstructBlockLayoutTestConstraintSpace(
    WritingDirectionMode writing_direction,
    LogicalSize size,
    bool stretch_inline_size_if_auto = true,
    bool is_new_formatting_context = false,
    LayoutUnit fragmentainer_space_available = kIndefiniteSize);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BASE_LAYOUT_ALGORITHM_TEST_H_
