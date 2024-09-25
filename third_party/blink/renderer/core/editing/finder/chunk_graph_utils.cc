// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/chunk_graph_utils.h"

#include "third_party/blink/renderer/core/dom/text.h"

namespace blink {

namespace {

constexpr LChar kAnyLevel[] = "*";

}  // namespace

void TextOrChar::Trace(Visitor* visitor) const {
  visitor->Trace(text);
}

CorpusChunk::CorpusChunk() : level_(String(kAnyLevel)) {}

CorpusChunk::CorpusChunk(const HeapVector<TextOrChar>& text_list,
                         const String& level)
    : level_(level) {
  text_list_ = text_list;
}

void CorpusChunk::Trace(Visitor* visitor) const {
  visitor->Trace(text_list_);
  visitor->Trace(next_list_);
}

void CorpusChunk::Link(CorpusChunk* next_chunk) {
  next_list_.push_back(next_chunk);
}

}  // namespace blink
