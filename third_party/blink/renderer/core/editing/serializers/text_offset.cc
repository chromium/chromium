// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/serializers/text_offset.h"

#include "third_party/blink/renderer/core/dom/text.h"

namespace blink {

TextOffset::TextOffset() : offset_(0) {}

TextOffset::TextOffset(Text* text, int offset) : text_(text), offset_(offset) {}

TextOffset::TextOffset(const TextOffset& other) = default;

bool TextOffset::IsNull() const {
  return !text_;
}

bool TextOffset::IsNotNull() const {
  return text_;
}

}  // namespace blink
