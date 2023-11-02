// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/blocking_attribute.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

// static
const char BlockingAttribute::kRenderToken[] = "render";

// static
HashSet<AtomicString>& BlockingAttribute::SupportedTokens() {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, tokens,
                      ({
                          kRenderToken,
                      }));

  return tokens;
}

// static
bool BlockingAttribute::HasRenderToken(const String& attribute_value) {
  if (!RuntimeEnabledFeatures::BlockingAttributeEnabled())
    return false;
  if (attribute_value.empty())
    return false;
  return SpaceSplitString(AtomicString(attribute_value)).Contains(kRenderToken);
}

bool BlockingAttribute::ValidateTokenValue(const AtomicString& token_value,
                                           ExceptionState&) const {
  DCHECK(RuntimeEnabledFeatures::BlockingAttributeEnabled());
  return SupportedTokens().Contains(token_value);
}

void BlockingAttribute::CountTokenUsage() {
  if (contains(kRenderToken)) {
    GetElement().GetDocument().CountUse(
        WebFeature::kBlockingAttributeRenderToken);
  }
}

}  // namespace blink
