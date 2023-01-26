// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timeline_offset.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"

namespace blink {

String TimelineOffset::ToString() const {
  if (name == NamedRange::kNone) {
    return "auto";
  }

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*MakeGarbageCollected<CSSIdentifierValue>(name));
  list->Append(*CSSValue::Create(offset, 1));
  return list->CssText();
}

}  // namespace blink
