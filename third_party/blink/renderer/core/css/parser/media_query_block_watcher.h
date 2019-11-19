// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_MEDIA_QUERY_BLOCK_WATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_MEDIA_QUERY_BLOCK_WATCHER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CSSParserToken;

class CORE_EXPORT MediaQueryBlockWatcher {
  STACK_ALLOCATED();

 public:
  MediaQueryBlockWatcher();
  void HandleToken(const CSSParserToken&);
  unsigned BlockLevel() const { return block_level_; }

 private:
  unsigned block_level_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_MEDIA_QUERY_BLOCK_WATCHER_H_
