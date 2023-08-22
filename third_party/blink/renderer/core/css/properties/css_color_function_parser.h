// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_COLOR_FUNCTION_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_COLOR_FUNCTION_PARSER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
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

 private:
  enum class ChannelType { kNone, kPercentage, kNumber, kRelative };
  bool ConsumeColorSpaceAndOriginColor(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       CSSParserTokenRange& args);
  bool ConsumeChannel(CSSParserTokenRange& args,
                      const CSSParserContext& context,
                      int index);
  bool ConsumeAlpha(CSSParserTokenRange& args, const CSSParserContext& context);
  bool MakePerColorSpaceAdjustments();

  Color::ColorSpace color_space_ = Color::ColorSpace::kNone;
  absl::optional<double> channels_[3];
  ChannelType channel_types_[3];
  absl::optional<double> alpha_ = 1.0;

  // Legacy colors have commas separating their channels. This syntax is
  // incompatible with CSSColor4 features like "none" or alpha with a slash.
  bool is_legacy_syntax_ = false;
  bool has_none_ = false;

  // For relative colors
  bool is_relative_color_ = false;
  Color origin_color_;
  HashMap<CSSValueID, double> channel_keyword_values_;
  HashMap<CSSValueID, double> xyz_keyword_values_;
  bool uses_rgb_relative_params_ = false;
  bool uses_xyz_relative_params_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_COLOR_FUNCTION_PARSER_H_
