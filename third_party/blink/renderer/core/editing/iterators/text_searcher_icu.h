// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_ITERATORS_TEXT_SEARCHER_ICU_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_ITERATORS_TEXT_SEARCHER_ICU_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/finder/find_options.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

struct UStringSearch;

namespace blink {

struct CORE_EXPORT MatchResultICU {
  wtf_size_t start;
  wtf_size_t length;
};

class CORE_EXPORT TextSearcherICU {
  DISALLOW_NEW();

 public:
  TextSearcherICU();
  ~TextSearcherICU();

  void SetPattern(const StringView& pattern, FindOptions options);
  void SetText(const UChar* text, wtf_size_t length);
  void SetOffset(wtf_size_t);
  bool NextMatchResult(MatchResultICU&);

 private:
  void SetPattern(const UChar* pattern, wtf_size_t length);
  void SetCaseSensitivity(bool case_sensitive);
  bool ShouldSkipCurrentMatch(MatchResultICU&) const;
  bool NextMatchResultInternal(MatchResultICU&);
  bool IsCorrectKanaMatch(const UChar* text, MatchResultICU&) const;

  UStringSearch* searcher_ = nullptr;
  wtf_size_t text_length_ = 0;
  Vector<UChar> normalized_search_text_;
  FindOptions options_;

  DISALLOW_COPY_AND_ASSIGN(TextSearcherICU);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_ITERATORS_TEXT_SEARCHER_ICU_H_
