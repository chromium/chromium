// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FRAGMENTATION_LAYOUT_INPUT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FRAGMENTATION_LAYOUT_INPUT_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class BlockBreakToken;
class ColumnSpannerPath;
class EarlyBreak;

struct FragmentationLayoutInput {
  STACK_ALLOCATED();

 public:
  FragmentationLayoutInput() = default;
  explicit FragmentationLayoutInput(
      const BlockBreakToken* break_token,
      const EarlyBreak* early_break = nullptr,
      const ColumnSpannerPath* column_spanner_path = nullptr)
      : break_token(break_token),
        early_break(early_break),
        column_spanner_path(column_spanner_path) {}

  const BlockBreakToken* break_token = nullptr;
  const EarlyBreak* early_break = nullptr;
  const ColumnSpannerPath* column_spanner_path = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FRAGMENTATION_LAYOUT_INPUT_H_
