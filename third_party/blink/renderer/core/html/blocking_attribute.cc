// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/blocking_attribute.h"

#include "base/no_destructor.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

const HashSet<AtomicString>& BlockingAttribute::SupportedTokens() const {
  static const base::NoDestructor<HashSet<AtomicString>> tokens({
      keywords::kRender,
  });
  return *tokens;
}

// static
bool BlockingAttribute::HasRenderToken(const String& attribute_value) {
  if (attribute_value.empty()) {
    return false;
  }
  return SpaceSplitString(AtomicString(attribute_value))
      .Contains(keywords::kRender);
}

bool BlockingAttribute::ValidateTokenValue(const AtomicString& token_value,
                                           ExceptionState&) const {
  return SupportedTokens().Contains(token_value);
}

void BlockingAttribute::OnAttributeValueChanged(const AtomicString& old_value,
                                                const AtomicString& new_value) {
  DidUpdateAttributeValue(old_value, new_value);
  if (contains(keywords::kRender)) {
    GetElement().GetDocument().CountUse(
        WebFeature::kBlockingAttributeRenderToken);
  }
}

}  // namespace blink
