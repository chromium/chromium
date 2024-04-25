// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_SAVE_POINT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_SAVE_POINT_H_

#include <type_traits>

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"

namespace blink {

// A generic RAII helper that allows you to rewind the parser if needed
// (e.g., if parsing fails after you've already consumed a few tokens).
// It is written generically so that it works for both the streaming
// and non-streaming parser with the same syntax.
//
// Rewind happens automatically in the destructor, unless you've called
// Release() to commit to the position in the stream.
template <class T>
  requires std::is_same_v<T, CSSParserTokenStream> ||
           std::is_same_v<T, CSSParserTokenRange>
class CSSParserSavePoint;

// Deduction guide to pick the correct template.
template <class T>
CSSParserSavePoint(T& stream_or_range) -> CSSParserSavePoint<T>;

template <>
class CSSParserSavePoint<CSSParserTokenStream> {
  STACK_ALLOCATED();

 public:
  explicit CSSParserSavePoint(CSSParserTokenStream& stream)
      : stream_(stream), savepoint_(stream.Save()) {}

  ~CSSParserSavePoint() {
    if (!released_) {
      stream_.Restore(savepoint_);
    }
  }

  void Release() {
    DCHECK(!released_);
    released_ = true;
  }

 private:
  CSSParserTokenStream& stream_;
  CSSParserTokenStream::State savepoint_;
  bool released_ = false;
};

template <>
class CSSParserSavePoint<CSSParserTokenRange> {
  STACK_ALLOCATED();

 public:
  explicit CSSParserSavePoint(CSSParserTokenRange& range)
      : range_(range), saved_range_(range) {}

  ~CSSParserSavePoint() {
    if (!released_) {
      range_ = saved_range_;
    }
  }

  void Release() {
    DCHECK(!released_);
    released_ = true;
  }

 private:
  CSSParserTokenRange& range_;
  CSSParserTokenRange saved_range_;
  bool released_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_SAVE_POINT_H_
