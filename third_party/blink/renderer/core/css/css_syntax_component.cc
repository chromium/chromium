// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_syntax_component.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

String CSSSyntaxComponent::ToString() const {
  StringBuilder builder;
  if (type_ == CSSSyntaxType::kIdent) {
    SerializeIdentifier(string_, builder);
  } else {
    builder.Append(blink::ToString(type_));
  }
  builder.Append(blink::ToString(repeat_));
  return builder.ReleaseString();
}

}  // namespace blink
