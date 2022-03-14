// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/blocking_attribute.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
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

bool BlockingAttribute::ValidateTokenValue(const AtomicString& token_value,
                                           ExceptionState&) const {
  DCHECK(RuntimeEnabledFeatures::BlockingAttributeEnabled());
  return SupportedTokens().Contains(token_value);
}

// static
bool BlockingAttribute::IsRenderBlocking(const AtomicString& attribute_value) {
  DCHECK(RuntimeEnabledFeatures::BlockingAttributeEnabled());
  if (attribute_value.IsEmpty())
    return false;
  return SpaceSplitString(attribute_value).Contains(kRenderToken);
}

}  // namespace blink
