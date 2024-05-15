// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhand.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"

namespace blink {

const CSSValue* Longhand::ParseSingleValue(
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_tokenizer) const {
  // Not all longhands are converted to using the streaming parser yet.
  // For them, we have an adapter (this function) that makes a token range
  // out of whatever is left, and then sends that to the older function.
  stream.EnsureLookAhead();
  CSSParserTokenStream::State saved_state = stream.Save();
  CSSParserTokenRange range = stream.ConsumeUntilPeekedTypeIs<>();
  CSSParserTokenRange range_copy = range;
  const CSSValue* value =
      ParseSingleValueFromRange(range_copy, context, local_tokenizer);

  if (!range_copy.AtEnd()) {
    // ParseSingleValueFromRange() didn't actually use all of the range.
    // This is fairly bad; we need to rewind the parser to the start
    // and then parse again to skip the right amount of tokens.
    stream.Restore(saved_state);
    ptrdiff_t tokens_to_skip =
        range_copy.RemainingSpan().data() - range.RemainingSpan().data();
    for (int i = 0; i < tokens_to_skip; ++i) {
      stream.ConsumeRaw();
    }
  }
  return value;
}

void Longhand::ApplyParentValue(StyleResolverState& state) const {
  // Creating the (computed) CSSValue involves unzooming using the parent's
  // effective zoom.
  const CSSValue* parent_computed_value =
      ComputedStyleUtils::ComputedPropertyValue(*this, *state.ParentStyle());
  CHECK(parent_computed_value);
  // Applying the CSSValue involves zooming using our effective zoom.
  ApplyValue(state, *parent_computed_value, ValueMode::kNormal);
}

bool Longhand::ApplyParentValueIfZoomChanged(StyleResolverState& state) const {
  if (state.ParentStyle()->EffectiveZoom() !=
      state.StyleBuilder().EffectiveZoom()) {
    ApplyParentValue(state);
    return true;
  }
  return false;
}

}  // namespace blink
