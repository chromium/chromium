// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_SCORE_LINE_BREAK_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_SCORE_LINE_BREAK_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_info_list.h"

namespace blink {

//
// Represents states and fields for `NGScoreLineBreaker` that should be kept
// across lines in an inline formatting context.
//
class CORE_EXPORT NGScoreLineBreakContext {
  STACK_ALLOCATED();

 public:
  NGLineInfoList& LineInfoList() { return line_info_list_; }

 private:
  NGLineInfoList line_info_list_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_SCORE_LINE_BREAK_CONTEXT_H_
