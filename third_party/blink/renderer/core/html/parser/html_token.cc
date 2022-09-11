// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_token.h"

namespace blink {

HTMLToken::AttributeList HTMLToken::CreateAttributeList() const {
  HTMLToken::AttributeList attributes;
  attributes.ReserveInitialCapacity(attribute_buffer_.NumberOfAttributes());
  for (HTMLAttributeBufferIterator buffer_iter(attribute_buffer_);
       !buffer_iter.AtEnd(); buffer_iter.Next()) {
    Attribute& attribute = attributes.emplace_back();
    attribute.NameBuffer().Append(buffer_iter.name());
    attribute.ValueBuffer().Append(buffer_iter.value());
    attribute.MutableNameRange().start = buffer_iter.name_range().start;
    attribute.MutableNameRange().end = buffer_iter.name_range().end;
    attribute.MutableValueRange().start = buffer_iter.value_range().start;
    attribute.MutableValueRange().end = buffer_iter.value_range().end;
  }
  return attributes;
}

}  // namespace blink
