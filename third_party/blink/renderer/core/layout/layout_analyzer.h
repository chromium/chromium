// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_ANALYZER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_ANALYZER_H_

#include <memory>
#include "base/macros.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class LayoutBlock;
class LayoutObject;
class TracedValue;

// Observes the performance of layout and reports statistics via a TracedValue.
// Usage:
// LayoutAnalyzer::Scope analyzer(*this);
class LayoutAnalyzer {
  USING_FAST_MALLOC(LayoutAnalyzer);

 public:
  enum Counter {
    kLayoutBlockWidthChanged,
    kLayoutBlockHeightChanged,
    kLayoutBlockSizeChanged,
    kLayoutBlockSizeDidNotChange,
    kLayoutObjectsThatSpecifyColumns,
    kLayoutAnalyzerStackMaximumDepth,
    kLayoutObjectsThatAreFloating,
    kLayoutObjectsThatHaveALayer,
    kLayoutInlineObjectsThatAlwaysCreateLineBoxes,
    kLayoutObjectsThatHadNeverHadLayout,
    kLayoutObjectsThatAreOutOfFlowPositioned,
    kLayoutObjectsThatNeedPositionedMovementLayout,
    kPerformLayoutRootLayoutObjects,
    kLayoutObjectsThatNeedLayoutForThemselves,
    kLayoutObjectsThatNeedSimplifiedLayout,
    kLayoutObjectsThatAreTableCells,
    kLayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath,
    kCharactersInLayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath,
    kLayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath,
    kCharactersInLayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath,
    kTotalLayoutObjectsThatWereLaidOut,
  };
  static const size_t kNumCounters = 21;

  class Scope {
    STACK_ALLOCATED();

   public:
    explicit Scope(const LayoutObject&);
    ~Scope();

   private:
    const LayoutObject& layout_object_;
    LayoutAnalyzer* analyzer_;
  };

  class BlockScope {
    STACK_ALLOCATED();

   public:
    explicit BlockScope(const LayoutBlock&);
    ~BlockScope();

   private:
    const LayoutBlock& block_;
    LayoutUnit width_;
    LayoutUnit height_;
  };

  LayoutAnalyzer() = default;

  void Reset();
  void Push(const LayoutObject&);
  void Pop(const LayoutObject&);

  void Increment(Counter counter, unsigned delta = 1) {
    counters_[counter] += delta;
  }

  std::unique_ptr<TracedValue> ToTracedValue();

 private:
  const char* NameForCounter(Counter) const;

  double start_ms_;
  unsigned depth_;
  unsigned counters_[kNumCounters];
  DISALLOW_COPY_AND_ASSIGN(LayoutAnalyzer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_ANALYZER_H_
