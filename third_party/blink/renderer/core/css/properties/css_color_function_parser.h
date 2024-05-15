// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_COLOR_FUNCTION_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_COLOR_FUNCTION_PARSER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

namespace blink {

class CORE_EXPORT ColorFunctionParser {
 public:
  ColorFunctionParser() = default;
  // Parses the color inputs rgb(), rgba(), hsl(), hsla(), hwb(), lab(),
  // oklab(), lch(), oklch() and color(). https://www.w3.org/TR/css-color-4/
  bool ConsumeFunctionalSyntaxColor(CSSParserTokenRange& input_range,
                                    const CSSParserContext& context,
                                    Color& result);
  bool ConsumeFunctionalSyntaxColor(CSSParserTokenStream& input_stream,
                                    const CSSParserContext& context,
                                    Color& result);

  struct FunctionMetadata;

 private:
  template <class T>
    requires std::is_same_v<T, CSSParserTokenStream> ||
             std::is_same_v<T, CSSParserTokenRange>
  bool ConsumeFunctionalSyntaxColorInternal(T& input_range,
                                            const CSSParserContext& context,
                                            Color& result);

  enum class ChannelType { kNone, kPercentage, kNumber, kRelative };
  bool ConsumeColorSpaceAndOriginColor(CSSParserTokenRange& args,
                                       CSSValueID function_id,
                                       const CSSParserContext& context);
  bool ConsumeChannel(CSSParserTokenRange& args,
                      const CSSParserContext& context,
                      int index);
  bool ConsumeAlpha(CSSParserTokenRange& args, const CSSParserContext& context);
  bool MakePerColorSpaceAdjustments();

  Color::ColorSpace color_space_ = Color::ColorSpace::kNone;
  std::optional<double> channels_[3];
  ChannelType channel_types_[3];
  std::optional<double> alpha_ = 1.0;

  // Metadata about the current function being parsed. Set by
  // `ConsumeColorSpaceAndOriginColor()` after parsing the preamble of the
  // function.
  const FunctionMetadata* function_metadata_ = nullptr;

  // Legacy colors have commas separating their channels. This syntax is
  // incompatible with CSSColor4 features like "none" or alpha with a slash.
  bool is_legacy_syntax_ = false;
  bool has_none_ = false;

  // For relative colors
  bool is_relative_color_ = false;
  Color origin_color_;
  HashMap<CSSValueID, double> channel_keyword_values_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_COLOR_FUNCTION_PARSER_H_
