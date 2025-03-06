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
HashSet<AtomicString>& BlockingAttribute::SupportedTokens() {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, tokens,
                      ({
                          keywords::kRender,
                      }));

  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, tokens_with_frame_rate,
                      ({
                          keywords::kRender,
                          keywords::kFullFrameRate,
                      }));

  if (RuntimeEnabledFeatures::RenderBlockingFullFrameRateEnabled()) {
    return tokens_with_frame_rate;
  }
  return tokens;
}

// static
bool BlockingAttribute::HasRenderToken(const String& attribute_value) {
  if (attribute_value.empty()) {
    return false;
  }
  return SpaceSplitString(AtomicString(attribute_value))
      .Contains(keywords::kRender);
}

// static
bool BlockingAttribute::HasFullFrameRateToken(const String& attribute_value) {
  if (attribute_value.empty()) {
    return false;
  }
  return SpaceSplitString(AtomicString(attribute_value))
      .Contains(keywords::kFullFrameRate);
}

bool BlockingAttribute::ValidateTokenValue(const AtomicString& token_value,
                                           ExceptionState&) const {
  return SupportedTokens().Contains(token_value);
}

RenderBlockingLevel BlockingAttribute::GetBlockingLevel() const {
  if (HasRenderToken()) {
    return RenderBlockingLevel::kBlock;
  }
  if (HasFullFrameRateToken() &&
      RuntimeEnabledFeatures::RenderBlockingFullFrameRateEnabled()) {
    return RenderBlockingLevel::kLimitFrameRate;
  }
  return RenderBlockingLevel::kNone;
}

void BlockingAttribute::OnAttributeValueChanged(const AtomicString& old_value,
                                                const AtomicString& new_value) {
  DidUpdateAttributeValue(old_value, new_value);
  // TODO(crbug.com/397832388): Add use counter for full-frame-rate
  if (contains(keywords::kRender)) {
    GetElement().GetDocument().CountUse(
        WebFeature::kBlockingAttributeRenderToken);
  }
}

}  // namespace blink
