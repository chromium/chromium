// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

NGBlockBreakToken::NGBlockBreakToken(
    NGLayoutInputNode node,
    LayoutUnit used_block_size,
    const NGBreakTokenVector& child_break_tokens,
    bool has_last_resort_break)
    : NGBreakToken(kBlockBreakToken, kUnfinished, node),
      used_block_size_(used_block_size),
      has_last_resort_break_(has_last_resort_break),
      num_children_(child_break_tokens.size()) {
  for (wtf_size_t i = 0; i < child_break_tokens.size(); ++i) {
    child_break_tokens_[i] = child_break_tokens[i].get();
    child_break_tokens_[i]->AddRef();
  }
}

NGBlockBreakToken::NGBlockBreakToken(NGLayoutInputNode node,
                                     LayoutUnit used_block_size,
                                     bool has_last_resort_break)
    : NGBreakToken(kBlockBreakToken, kFinished, node),
      used_block_size_(used_block_size),
      has_last_resort_break_(has_last_resort_break),
      num_children_(0) {}

NGBlockBreakToken::NGBlockBreakToken(NGLayoutInputNode node)
    : NGBreakToken(kBlockBreakToken, kUnfinished, node), num_children_(0) {}

#ifndef NDEBUG

String NGBlockBreakToken::ToString() const {
  StringBuilder string_builder;
  string_builder.Append(NGBreakToken::ToString());
  string_builder.Append(" used:");
  string_builder.Append(used_block_size_.ToString());
  string_builder.Append("px");
  return string_builder.ToString();
}

#endif  // NDEBUG

}  // namespace blink
