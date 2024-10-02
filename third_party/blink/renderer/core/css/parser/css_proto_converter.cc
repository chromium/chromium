// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/parser/css_proto_converter.h"
#include <string>

// TODO(metzman): Figure out how to remove this include and use DCHECK.
#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/css/parser/css.pb.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

// TODO(bikineev): "IN" comes as a macro from <windows.h>. It conflicts with
// Length::IN from the generated proto file. Change the name in css.proto rather
// than hacking with directives here.
#if BUILDFLAG(IS_WIN) && defined(IN)
#undef IN
#endif

namespace css_proto_converter {

const int Converter::kAtRuleDepthLimit = 5;
const int Converter::kSupportsConditionDepthLimit = 5;

const std::string Converter::kViewportPropertyLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "min-width",  "max-width", "width",       "min-height",
    "max-height", "height",    "zoom",        "min-zoom",
    "user-zoom",  "max-zoom",  "orientation",
};

const std::string Converter::kViewportValueLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "landscape", "portrait", "auto", "zoom", "fixed", "none",
};

const std::string Converter::kPseudoLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "-internal-autofill-previewed",
    "-internal-autofill-selected",
    "-internal-dialog-in-top-layer",
    "-internal-is-html",
    "-internal-list-box",
    "-internal-media-controls-overlay-cast-button",
    "-internal-multi-select-focus",
    "-internal-popover-in-top-layer",
    "-internal-shadow-host-has-non-auto-appearance",
    "-internal-spatial-navigation-focus",
    "-internal-video-persistent",
    "-internal-video-persistent-ancestor",
    "-webkit-any-link",
    "-webkit-autofill",
    "-webkit-drag",
    "-webkit-full-page-media",
    "-webkit-full-screen",
    "-webkit-full-screen-ancestor",
    "-webkit-resizer",
    "-webkit-scrollbar",
    "-webkit-scrollbar-button",
    "-webkit-scrollbar-corner",
    "-webkit-scrollbar-thumb",
    "-webkit-scrollbar-track",
    "-webkit-scrollbar-track-piece",
    "active",
    "active-view-transition",
    "active-view-transition-type",
    "after",
    "autofill",
    "backdrop",
    "before",
    "checked",
    "closed",
    "corner-present",
    "cue",
    "decrement",
    "default",
    "defined",
    "disabled",
    "double-button",
    "empty",
    "enabled",
    "end",
    "first",
    "first-child",
    "first-letter",
    "first-line",
    "first-of-type",
    "focus",
    "focus-within",
    "fullscreen",
    "future",
    "has-slotted",
    "horizontal",
    "host",
    "hover",
    "in-range",
    "increment",
    "indeterminate",
    "invalid",
    "last-child",
    "last-of-type",
    "left",
    "link",
    "no-button",
    "only-child",
    "only-of-type",
    "open",
    "optional",
    "out-of-range",
    "past",
    "placeholder",
    "placeholder-shown",
    "popover-open",
    "read-only",
    "read-write",
    "required",
    "right",
    "root",
    "scope",
    "selection",
    "single-button",
    "start",
    "state",
    "target",
    "user-invalid",
    "user-valid",
    "valid",
    "vertical",
    "visited",
    "window-inactive",
    "-webkit-any",
    "host-context",
    "lang",
    "not",
    "nth-child",
    "nth-last-child",
    "nth-last-of-type",
    "nth-of-type",
    "slotted",
    "xr-overlay",
    "INVALID_PSEUDO_VALUE"};

const std::string Converter::kMediaTypeLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "all",
    "braille",
    "embossed",
    "handheld",
    "print",
    "projection",
    "screen",
    "speech",
    "tty",
    "tv",
    "INVALID_MEDIA_TYPE"};

const std::string Converter::kMfNameLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "any-hover",
    "any-pointer",
    "color",
    "color-index",
    "color-gamut",
    "grid",
    "monochrome",
    "height",
    "hover",
    "width",
    "orientation",
    "aspect-ratio",
    "device-aspect-ratio",
    "-webkit-device-pixel-ratio",
    "device-height",
    "device-width",
    "display-mode",
    "max-color",
    "max-color-index",
    "max-aspect-ratio",
    "max-device-aspect-ratio",
    "-webkit-max-device-pixel-ratio",
    "max-device-height",
    "max-device-width",
    "max-height",
    "max-monochrome",
    "max-width",
    "max-resolution",
    "min-color",
    "min-color-index",
    "min-aspect-ratio",
    "min-device-aspect-ratio",
    "-webkit-min-device-pixel-ratio",
    "min-device-height",
    "min-device-width",
    "min-height",
    "min-monochrome",
    "min-width",
    "min-resolution",
    "pointer",
    "resolution",
    "-webkit-transform-3d",
    "scan",
    "shape",
    "immersive",
    "dynamic-range",
    "video-dynamic-range",
    "INVALID_NAME"};

const std::string Converter::kImportLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "'custom.css'", "url(\"chrome://communicator/skin/\")"};

const std::string Converter::kEncodingLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "UTF-8",
    "UTF-16",
    "UTF-32",
};

#include "third_party/blink/renderer/core/css/parser/css_proto_converter_generated.h"

Converter::Converter() = default;

std::string Converter::Convert(const StyleSheet& style_sheet_message) {
  Reset();
  Visit(style_sheet_message);
  return string_;
}

void Converter::Visit(const Unicode& unicode) {
  string_ += "\\";
  string_ += static_cast<char>(unicode.ascii_value_1());

  if (unicode.has_ascii_value_2()) {
    string_ += static_cast<char>(unicode.ascii_value_2());
  }
  if (unicode.has_ascii_value_3()) {
    string_ += static_cast<char>(unicode.ascii_value_3());
  }
  if (unicode.has_ascii_value_4()) {
    string_ += static_cast<char>(unicode.ascii_value_4());
  }
  if (unicode.has_ascii_value_5()) {
    string_ += static_cast<char>(unicode.ascii_value_5());
  }
  if (unicode.has_ascii_value_6()) {
    string_ += static_cast<char>(unicode.ascii_value_6());
  }

  if (unicode.has_unrepeated_w()) {
    Visit(unicode.unrepeated_w());
  }
}

void Converter::Visit(const Escape& escape) {
  if (escape.has_ascii_value()) {
    string_ += "\\";
    string_ += static_cast<char>(escape.ascii_value());
  } else if (escape.has_unicode()) {
    Visit(escape.unicode());
  }
}

void Converter::Visit(const Nmstart& nmstart) {
  if (nmstart.has_ascii_value()) {
    string_ += static_cast<char>(nmstart.ascii_value());
  } else if (nmstart.has_escape()) {
    Visit(nmstart.escape());
  }
}

void Converter::Visit(const Nmchar& nmchar) {
  if (nmchar.has_ascii_value()) {
    string_ += static_cast<char>(nmchar.ascii_value());
  } else if (nmchar.has_escape()) {
    Visit(nmchar.escape());
  }
}

void Converter::Visit(const String& string) {
  bool use_single_quotes = string.use_single_quotes();
  if (use_single_quotes) {
    string_ += "'";
  } else {
    string_ += "\"";
  }

  for (auto& string_char_quote : string.string_char_quotes()) {
    Visit(string_char_quote, use_single_quotes);
  }

  if (use_single_quotes) {
    string_ += "'";
  } else {
    string_ += "\"";
  }
}

void Converter::Visit(const StringCharOrQuote& string_char_quote,
                      bool using_single_quote) {
  if (string_char_quote.has_string_char()) {
    Visit(string_char_quote.string_char());
  } else if (string_char_quote.quote_char()) {
    if (using_single_quote) {
      string_ += "\"";
    } else {
      string_ += "'";
    }
  }
}

void Converter::Visit(const StringChar& string_char) {
  if (string_char.has_url_char()) {
    Visit(string_char.url_char());
  } else if (string_char.has_space()) {
    string_ += " ";
  } else if (string_char.has_nl()) {
    Visit(string_char.nl());
  }
}

void Converter::Visit(const Ident& ident) {
  if (ident.starting_minus()) {
    string_ += "-";
  }
  Visit(ident.nmstart());
  for (auto& nmchar : ident.nmchars()) {
    Visit(nmchar);
  }
}

void Converter::Visit(const Num& num) {
  if (num.has_float_value()) {
    string_ += std::to_string(num.float_value());
  } else {
    string_ += std::to_string(num.signed_int_value());
  }
}

void Converter::Visit(const UrlChar& url_char) {
  string_ += static_cast<char>(url_char.ascii_value());
}

// TODO(metzman): implement W
void Converter::Visit(const UnrepeatedW& unrepeated_w) {
  string_ += static_cast<char>(unrepeated_w.ascii_value());
}

void Converter::Visit(const Nl& nl) {
  string_ += "\\";
  if (nl.newline_kind() == Nl::CR_LF) {
    string_ += "\r\n";
  } else {  // Otherwise newline_kind is the ascii value of the char we want.
    string_ += static_cast<char>(nl.newline_kind());
  }
}

void Converter::Visit(const Length& length) {
  Visit(length.num());
  if (length.unit() == Length::PX) {
    string_ += "px";
  } else if (length.unit() == Length::CM) {
    string_ += "cm";
  } else if (length.unit() == Length::MM) {
    string_ += "mm";
  } else if (length.unit() == Length::IN) {
    string_ += "in";
  } else if (length.unit() == Length::PT) {
    string_ += "pt";
  } else if (length.unit() == Length::PC) {
    string_ += "pc";
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void Converter::Visit(const Angle& angle) {
  Visit(angle.num());
  if (angle.unit() == Angle::DEG) {
    string_ += "deg";
  } else if (angle.unit() == Angle::RAD) {
    string_ += "rad";
  } else if (angle.unit() == Angle::GRAD) {
    string_ += "grad";
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void Converter::Visit(const Time& time) {
  Visit(time.num());
  if (time.unit() == Time::MS) {
    string_ += "ms";
  } else if (time.unit() == Time::S) {
    string_ += "s";
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void Converter::Visit(const Freq& freq) {
  Visit(freq.num());
  // Hack around really dumb build bug
  if (freq.unit() == Freq::_HZ) {
    string_ += "Hz";
  } else if (freq.unit() == Freq::KHZ) {
    string_ += "kHz";
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void Converter::Visit(const Uri& uri) {
  string_ += "url(\"chrome://communicator/skin/\");";
}

void Converter::Visit(const FunctionToken& function_token) {
  Visit(function_token.ident());
  string_ += "(";
}

void Converter::Visit(const StyleSheet& style_sheet) {
  if (style_sheet.has_charset_declaration()) {
    Visit(style_sheet.charset_declaration());
  }
  for (auto& import : style_sheet.imports()) {
    Visit(import);
  }
  for (auto& _namespace : style_sheet.namespaces()) {
    Visit(_namespace);
  }
  for (auto& nested_at_rule : style_sheet.nested_at_rules()) {
    Visit(nested_at_rule);
  }
}

void Converter::Visit(const ViewportValue& viewport_value) {
  if (viewport_value.has_length()) {
    Visit(viewport_value.length());
  } else if (viewport_value.has_num()) {
    Visit(viewport_value.num());
  } else {  // Default value.
    AppendTableValue<ViewportValue_ValueId_ValueId_ARRAYSIZE>(
        viewport_value.value_id(), kViewportValueLookupTable);
  }
}

void Converter::Visit(const Viewport& viewport) {
  string_ += " @viewport {";
  for (auto& property_and_value : viewport.properties_and_values()) {
    AppendPropertyAndValue<ViewportProperty_PropertyId_PropertyId_ARRAYSIZE>(
        property_and_value, kViewportPropertyLookupTable);
  }
  string_ += " } ";
}

void Converter::Visit(const CharsetDeclaration& charset_declaration) {
  string_ += "@charset ";  // CHARSET_SYM
  string_ += "\"";
  AppendTableValue<CharsetDeclaration_EncodingId_EncodingId_ARRAYSIZE>(
      charset_declaration.encoding_id(), kEncodingLookupTable);
  string_ += "\"; ";
}

void Converter::Visit(const AtRuleOrRulesets& at_rule_or_rulesets, int depth) {
  Visit(at_rule_or_rulesets.first(), depth);
  for (auto& later : at_rule_or_rulesets.laters()) {
    Visit(later, depth);
  }
}

void Converter::Visit(const AtRuleOrRuleset& at_rule_or_ruleset, int depth) {
  if (at_rule_or_ruleset.has_at_rule()) {
    Visit(at_rule_or_ruleset.at_rule(), depth);
  } else {  // Default.
    Visit(at_rule_or_ruleset.ruleset());
  }
}

void Converter::Visit(const NestedAtRule& nested_at_rule, int depth) {
  if (++depth > kAtRuleDepthLimit) {
    return;
  }

  if (nested_at_rule.has_ruleset()) {
    Visit(nested_at_rule.ruleset());
  } else if (nested_at_rule.has_media()) {
    Visit(nested_at_rule.media());
  } else if (nested_at_rule.has_viewport()) {
    Visit(nested_at_rule.viewport());
  } else if (nested_at_rule.has_supports_rule()) {
    Visit(nested_at_rule.supports_rule(), depth);
  }
  // Else apppend nothing.
  // TODO(metzman): Support pages and font-faces.
}

void Converter::Visit(const SupportsRule& supports_rule, int depth) {
  string_ += "@supports ";
  Visit(supports_rule.supports_condition(), depth);
  string_ += " { ";
  for (auto& at_rule_or_ruleset : supports_rule.at_rule_or_rulesets()) {
    Visit(at_rule_or_ruleset, depth);
  }
  string_ += " } ";
}

void Converter::AppendBinarySupportsCondition(
    const BinarySupportsCondition& binary_condition,
    std::string binary_operator,
    int depth) {
  Visit(binary_condition.condition_1(), depth);
  string_ += " " + binary_operator + " ";
  Visit(binary_condition.condition_2(), depth);
}

void Converter::Visit(const SupportsCondition& supports_condition, int depth) {
  bool under_depth_limit = ++depth <= kSupportsConditionDepthLimit;

  if (supports_condition.not_condition()) {
    string_ += " not ";
  }

  string_ += "(";

  if (under_depth_limit && supports_condition.has_and_supports_condition()) {
    AppendBinarySupportsCondition(supports_condition.or_supports_condition(),
                                  "and", depth);
  } else if (under_depth_limit &&
             supports_condition.has_or_supports_condition()) {
    AppendBinarySupportsCondition(supports_condition.or_supports_condition(),
                                  "or", depth);
  } else {
    // Use the required property_and_value field if the or_supports_condition
    // and and_supports_condition are unset or if we have reached the depth
    // limit and don't want another nested condition.
    Visit(supports_condition.property_and_value());
  }

  string_ += ")";
}

void Converter::Visit(const Import& import) {
  string_ += "@import ";
  AppendTableValue<Import_SrcId_SrcId_ARRAYSIZE>(import.src_id(),
                                                 kImportLookupTable);
  string_ += " ";
  if (import.has_media_query_list()) {
    Visit(import.media_query_list());
  }
  string_ += "; ";
}

void Converter::Visit(const MediaQueryList& media_query_list) {
  bool first = true;
  for (auto& media_query : media_query_list.media_queries()) {
    if (first) {
      first = false;
    } else {
      string_ += ", ";
    }
    Visit(media_query);
  }
}

void Converter::Visit(const MediaQuery& media_query) {
  if (media_query.has_media_query_part_two()) {
    Visit(media_query.media_query_part_two());
  } else {
    Visit(media_query.media_condition());
  }
}

void Converter::Visit(const MediaQueryPartTwo& media_query_part_two) {
  if (media_query_part_two.has_not_or_only()) {
    if (media_query_part_two.not_or_only() == MediaQueryPartTwo::NOT) {
      string_ += " not ";
    } else {
      string_ += " only ";
    }
  }
  Visit(media_query_part_two.media_type());
  if (media_query_part_two.has_media_condition_without_or()) {
    string_ += " and ";
    Visit(media_query_part_two.media_condition_without_or());
  }
}

void Converter::Visit(const MediaCondition& media_condition) {
  if (media_condition.has_media_not()) {
    Visit(media_condition.media_not());
  } else if (media_condition.has_media_or()) {
    Visit(media_condition.media_or());
  } else if (media_condition.has_media_in_parens()) {
    Visit(media_condition.media_in_parens());
  } else {
    Visit(media_condition.media_and());
  }
}

void Converter::Visit(const MediaConditionWithoutOr& media_condition) {
  if (media_condition.has_media_and()) {
    Visit(media_condition.media_and());
  } else if (media_condition.has_media_in_parens()) {
    Visit(media_condition.media_in_parens());
  } else {
    Visit(media_condition.media_not());
  }
}

void Converter::Visit(const MediaType& media_type) {
  AppendTableValue<MediaType_ValueId_ValueId_ARRAYSIZE>(media_type.value_id(),
                                                        kMediaTypeLookupTable);
}

void Converter::Visit(const MediaNot& media_not) {
  string_ += " not ";
  Visit(media_not.media_in_parens());
}

void Converter::Visit(const MediaAnd& media_and) {
  Visit(media_and.first_media_in_parens());
  string_ += " and ";
  Visit(media_and.second_media_in_parens());
  for (auto& media_in_parens : media_and.media_in_parens_list()) {
    string_ += " and ";
    Visit(media_in_parens);
  }
}

void Converter::Visit(const MediaOr& media_or) {
  Visit(media_or.first_media_in_parens());
  string_ += " or ";
  Visit(media_or.second_media_in_parens());
  for (auto& media_in_parens : media_or.media_in_parens_list()) {
    string_ += " or ";
    Visit(media_in_parens);
  }
}

void Converter::Visit(const MediaInParens& media_in_parens) {
  if (media_in_parens.has_media_condition()) {
    string_ += " (";
    Visit(media_in_parens.media_condition());
    string_ += " )";
  } else if (media_in_parens.has_media_feature()) {
    Visit(media_in_parens.media_feature());
  }
}

void Converter::Visit(const MediaFeature& media_feature) {
  string_ += "(";
  if (media_feature.has_mf_bool()) {
    Visit(media_feature.mf_bool());
  } else if (media_feature.has_mf_plain()) {
    AppendPropertyAndValue<MfName_ValueId_ValueId_ARRAYSIZE>(
        media_feature.mf_plain(), kMfNameLookupTable, false);
  }
  string_ += ")";
}

void Converter::Visit(const MfBool& mf_bool) {
  Visit(mf_bool.mf_name());
}

void Converter::Visit(const MfName& mf_name) {
  AppendTableValue<MfName_ValueId_ValueId_ARRAYSIZE>(mf_name.id(),
                                                     kMfNameLookupTable);
}

void Converter::Visit(const MfValue& mf_value) {
  if (mf_value.has_length()) {
    Visit(mf_value.length());
  } else if (mf_value.has_ident()) {
    Visit(mf_value.ident());
  } else {
    Visit(mf_value.num());
  }
}

void Converter::Visit(const Namespace& _namespace) {
  string_ += "@namespace ";
  if (_namespace.has_namespace_prefix()) {
    Visit(_namespace.namespace_prefix());
  }
  if (_namespace.has_string()) {
    Visit(_namespace.string());
  }
  if (_namespace.has_uri()) {
    Visit(_namespace.uri());
  }

  string_ += "; ";
}

void Converter::Visit(const NamespacePrefix& namespace_prefix) {
  Visit(namespace_prefix.ident());
}

void Converter::Visit(const Media& media) {
  // MEDIA_SYM S*
  string_ += "@media ";  // "@media" {return MEDIA_SYM;}

  Visit(media.media_query_list());
  string_ += " { ";
  for (auto& ruleset : media.rulesets()) {
    Visit(ruleset);
  }
  string_ += " } ";
}

void Converter::Visit(const Page& page) {
  // PAGE_SYM
  string_ += "@page ";  // PAGE_SYM
  if (page.has_ident()) {
    Visit(page.ident());
  }
  if (page.has_pseudo_page()) {
    Visit(page.pseudo_page());
  }
  string_ += " { ";
  Visit(page.declaration_list());
  string_ += " } ";
}

void Converter::Visit(const PseudoPage& pseudo_page) {
  string_ += ":";
  Visit(pseudo_page.ident());
}

void Converter::Visit(const DeclarationList& declaration_list) {
  Visit(declaration_list.first_declaration());
  for (auto& declaration : declaration_list.later_declarations()) {
    Visit(declaration);
    string_ += "; ";
  }
}

void Converter::Visit(const FontFace& font_face) {
  string_ += "@font-face";
  string_ += "{";
  // Visit(font_face.declaration_list());
  string_ += "}";
}

void Converter::Visit(const Operator& _operator) {
  if (_operator.has_ascii_value()) {
    string_ += static_cast<char>(_operator.ascii_value());
  }
}

void Converter::Visit(const UnaryOperator& unary_operator) {
  string_ += static_cast<char>(unary_operator.ascii_value());
}

void Converter::Visit(const Property& property) {
  AppendTableValue<Property_NameId_NameId_ARRAYSIZE>(property.name_id(),
                                                     kPropertyLookupTable);
}

void Converter::Visit(const Ruleset& ruleset) {
  Visit(ruleset.selector_list());
  string_ += " {";
  Visit(ruleset.declaration_list());
  string_ += "} ";
}

void Converter::Visit(const SelectorList& selector_list) {
  Visit(selector_list.first_selector(), true);
  for (auto& selector : selector_list.later_selectors()) {
    Visit(selector, false);
  }
  string_ += " ";
}

// Also visits Attr
void Converter::Visit(const Selector& selector, bool is_first) {
  if (!is_first) {
    string_ += " ";
    if (selector.combinator() != Combinator::NONE) {
      string_ += static_cast<char>(selector.combinator());
      string_ += " ";
    }
  }
  if (selector.type() == Selector::ELEMENT) {
    string_ += "a";
  } else if (selector.type() == Selector::CLASS) {
    string_ += ".classname";
  } else if (selector.type() == Selector::ID) {
    string_ += "#idname";
  } else if (selector.type() == Selector::UNIVERSAL) {
    string_ += "*";
  } else if (selector.type() == Selector::ATTR) {
    std::string val1 = "href";
    std::string val2 = ".org";
    string_ += "a[" + val1;
    if (selector.attr().type() != Attr::NONE) {
      string_ += " ";
      string_ += static_cast<char>(selector.attr().type());
      string_ += +"= " + val2;
    }
    if (selector.attr().attr_i()) {
      string_ += " i";
    }
    string_ += "]";
  }
  if (selector.has_pseudo_value_id()) {
    string_ += ":";
    if (selector.pseudo_type() == PseudoType::ELEMENT) {
      string_ += ":";
    }
    AppendTableValue<Selector_PseudoValueId_PseudoValueId_ARRAYSIZE>(
        selector.pseudo_value_id(), kPseudoLookupTable);
  }
}

void Converter::Visit(const Declaration& declaration) {
  if (declaration.has_property_and_value()) {
    Visit(declaration.property_and_value());
  }
  // else empty
}

void Converter::Visit(const PropertyAndValue& property_and_value) {
  Visit(property_and_value.property());
  string_ += " : ";
  int value_id = 0;
  if (property_and_value.has_value_id()) {
    value_id = property_and_value.value_id();
  }
  Visit(property_and_value.expr(), value_id);
  if (property_and_value.has_prio()) {
    string_ += " !important ";
  }
}

void Converter::Visit(const Expr& expr, int declaration_value_id) {
  if (!declaration_value_id) {
    Visit(expr.term());
  } else {
    AppendTableValue<PropertyAndValue_ValueId_ValueId_ARRAYSIZE>(
        declaration_value_id, kValueLookupTable);
  }
  for (auto& operator_term : expr.operator_terms()) {
    Visit(operator_term);
  }
}

void Converter::Visit(const OperatorTerm& operator_term) {
  Visit(operator_term._operator());
  Visit(operator_term.term());
}

void Converter::Visit(const Term& term) {
  if (term.has_unary_operator()) {
    Visit(term.unary_operator());
  }

  if (term.has_term_part()) {
    Visit(term.term_part());
  } else if (term.has_string()) {
    Visit(term.string());
  }

  if (term.has_ident()) {
    Visit(term.ident());
  }
  if (term.has_uri()) {
    Visit(term.uri());
  }
  if (term.has_hexcolor()) {
    Visit(term.hexcolor());
  }
}

void Converter::Visit(const TermPart& term_part) {
  if (term_part.has_number()) {
    Visit(term_part.number());
  }
  // S* | PERCENTAGE
  if (term_part.has_percentage()) {
    Visit(term_part.percentage());
    string_ += "%";
  }
  // S* | LENGTH
  if (term_part.has_length()) {
    Visit(term_part.length());
  }
  // S* | EMS
  if (term_part.has_ems()) {
    Visit(term_part.ems());
    string_ += "em";
  }
  // S* | EXS
  if (term_part.has_exs()) {
    Visit(term_part.exs());
    string_ += "ex";
  }
  // S* | Angle
  if (term_part.has_angle()) {
    Visit(term_part.angle());
  }
  // S* | TIME
  if (term_part.has_time()) {
    Visit(term_part.time());
  }
  // S* | FREQ
  if (term_part.has_freq()) {
    Visit(term_part.freq());
  }
  // S* | function
  if (term_part.has_function()) {
    Visit(term_part.function());
  }
}

void Converter::Visit(const Function& function) {
  Visit(function.function_token());
  Visit(function.expr());
  string_ += ")";
}

void Converter::Visit(const Hexcolor& hexcolor) {
  string_ += "#";
  Visit(hexcolor.first_three());
  if (hexcolor.has_last_three()) {
    Visit(hexcolor.last_three());
  }
}

void Converter::Visit(const HexcolorThree& hexcolor_three) {
  string_ += static_cast<char>(hexcolor_three.ascii_value_1());
  string_ += static_cast<char>(hexcolor_three.ascii_value_2());
  string_ += static_cast<char>(hexcolor_three.ascii_value_3());
}

void Converter::Reset() {
  string_.clear();
}

template <size_t EnumSize, size_t TableSize>
void Converter::AppendTableValue(int id,
                                 const std::string (&lookup_table)[TableSize]) {
  // If you hit this assert, you likely need to modify
  // css/parser/templates/css.proto.tmpl.
  static_assert(EnumSize == TableSize,
                "Enum used as index should not overflow lookup table");
  CHECK(id > 0 && static_cast<size_t>(id) < TableSize);
  string_ += lookup_table[id];
}

template <size_t EnumSize, class T, size_t TableSize>
void Converter::AppendPropertyAndValue(
    T property_and_value,
    const std::string (&lookup_table)[TableSize],
    bool append_semicolon) {
  AppendTableValue<EnumSize>(property_and_value.property().id(), lookup_table);
  string_ += " : ";
  Visit(property_and_value.value());
  if (append_semicolon) {
    string_ += "; ";
  }
}
}  // namespace css_proto_converter
