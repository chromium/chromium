// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_analyzer.h"

#include <memory>
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

LayoutAnalyzer::Scope::Scope(const LayoutObject& o)
    : layout_object_(o), analyzer_(o.GetFrameView()->GetLayoutAnalyzer()) {
  if (analyzer_)
    analyzer_->Push(o);
}

LayoutAnalyzer::Scope::~Scope() {
  if (analyzer_)
    analyzer_->Pop(layout_object_);
}

LayoutAnalyzer::BlockScope::BlockScope(const LayoutBlock& block)
    : block_(block),
      width_(block.FrameRect().Width()),
      height_(block.FrameRect().Height()) {}

LayoutAnalyzer::BlockScope::~BlockScope() {
  LayoutAnalyzer* analyzer = block_.GetFrameView()->GetLayoutAnalyzer();
  if (!analyzer)
    return;
  bool changed = false;
  if (width_ != block_.FrameRect().Width()) {
    analyzer->Increment(kLayoutBlockWidthChanged);
    changed = true;
  }
  if (height_ != block_.FrameRect().Height()) {
    analyzer->Increment(kLayoutBlockHeightChanged);
    changed = true;
  }
  analyzer->Increment(changed ? kLayoutBlockSizeChanged
                              : kLayoutBlockSizeDidNotChange);
}

void LayoutAnalyzer::Reset() {
  depth_ = 0;
  for (size_t i = 0; i < kNumCounters; ++i) {
    counters_[i] = 0;
  }
}

void LayoutAnalyzer::Push(const LayoutObject& o) {
  Increment(kTotalLayoutObjectsThatWereLaidOut);
  if (!o.EverHadLayout())
    Increment(kLayoutObjectsThatHadNeverHadLayout);
  if (o.SelfNeedsLayout())
    Increment(kLayoutObjectsThatNeedLayoutForThemselves);
  if (o.NeedsPositionedMovementLayout())
    Increment(kLayoutObjectsThatNeedPositionedMovementLayout);
  if (o.IsOutOfFlowPositioned())
    Increment(kLayoutObjectsThatAreOutOfFlowPositioned);
  if (o.IsTableCell())
    Increment(kLayoutObjectsThatAreTableCells);
  if (o.IsFloating())
    Increment(kLayoutObjectsThatAreFloating);
  if (o.StyleRef().SpecifiesColumns())
    Increment(kLayoutObjectsThatSpecifyColumns);
  if (o.HasLayer())
    Increment(kLayoutObjectsThatHaveALayer);
  if (o.IsLayoutInline() && o.AlwaysCreateLineBoxesForLayoutInline())
    Increment(kLayoutInlineObjectsThatAlwaysCreateLineBoxes);
  if (o.IsText()) {
    const LayoutText& t = *ToLayoutText(&o);
    Increment(kLayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath);
    Increment(
        kCharactersInLayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath,
        t.TextLength());
  }

  ++depth_;

  // This refers to LayoutAnalyzer depth, which is generally closer to C++
  // stack recursion depth, not layout tree depth or DOM tree depth.
  counters_[kLayoutAnalyzerStackMaximumDepth] =
      max(counters_[kLayoutAnalyzerStackMaximumDepth], depth_);
}

void LayoutAnalyzer::Pop(const LayoutObject& o) {
  DCHECK_GT(depth_, 0u);
  --depth_;
}

std::unique_ptr<TracedValue> LayoutAnalyzer::ToTracedValue() {
  auto traced_value(std::make_unique<TracedValue>());
  for (size_t i = 0; i < kNumCounters; ++i) {
    if (counters_[i] > 0) {
      traced_value->SetIntegerWithCopiedName(
          NameForCounter(static_cast<Counter>(i)), counters_[i]);
    }
  }
  return traced_value;
}

const char* LayoutAnalyzer::NameForCounter(Counter counter) const {
  switch (counter) {
    case kLayoutBlockWidthChanged:
      return "LayoutBlockWidthChanged";
    case kLayoutBlockHeightChanged:
      return "LayoutBlockHeightChanged";
    case kLayoutBlockSizeChanged:
      return "LayoutBlockSizeChanged";
    case kLayoutBlockSizeDidNotChange:
      return "LayoutBlockSizeDidNotChange";
    case kLayoutObjectsThatSpecifyColumns:
      return "LayoutObjectsThatSpecifyColumns";
    case kLayoutAnalyzerStackMaximumDepth:
      return "LayoutAnalyzerStackMaximumDepth";
    case kLayoutObjectsThatAreFloating:
      return "LayoutObjectsThatAreFloating";
    case kLayoutObjectsThatHaveALayer:
      return "LayoutObjectsThatHaveALayer";
    case kLayoutInlineObjectsThatAlwaysCreateLineBoxes:
      return "LayoutInlineObjectsThatAlwaysCreateLineBoxes";
    case kLayoutObjectsThatHadNeverHadLayout:
      return "LayoutObjectsThatHadNeverHadLayout";
    case kLayoutObjectsThatAreOutOfFlowPositioned:
      return "LayoutObjectsThatAreOutOfFlowPositioned";
    case kLayoutObjectsThatNeedPositionedMovementLayout:
      return "LayoutObjectsThatNeedPositionedMovementLayout";
    case kPerformLayoutRootLayoutObjects:
      return "PerformLayoutRootLayoutObjects";
    case kLayoutObjectsThatNeedLayoutForThemselves:
      return "LayoutObjectsThatNeedLayoutForThemselves";
    case kLayoutObjectsThatNeedSimplifiedLayout:
      return "LayoutObjectsThatNeedSimplifiedLayout";
    case kLayoutObjectsThatAreTableCells:
      return "LayoutObjectsThatAreTableCells";
    case kLayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath:
      return "LayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath";
    case kCharactersInLayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath:
      return "CharactersInLayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCode"
             "Path";
    case kLayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath:
      return "LayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath";
    case kCharactersInLayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath:
      return "CharactersInLayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePat"
             "h";
    case kTotalLayoutObjectsThatWereLaidOut:
      return "TotalLayoutObjectsThatWereLaidOut";
  }
  NOTREACHED();
  return "";
}

}  // namespace blink
