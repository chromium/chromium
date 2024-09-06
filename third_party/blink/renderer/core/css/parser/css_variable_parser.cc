// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"

#include "base/containers/contains.h"
#include "third_party/blink/renderer/core/css/css_attr_type.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

bool IsValidVariableReference(CSSParserTokenRange, const ExecutionContext*);
bool IsValidEnvVariableReference(CSSParserTokenRange, const ExecutionContext*);
bool IsValidAttributeReference(CSSParserTokenRange, const ExecutionContext*);

// Checks if a token sequence is a valid <declaration-value> [1],
// with the additional restriction that any var()/env() functions (if present)
// must follow their respective grammars as well.
//
// If this function returns true, then it outputs some additional details about
// the token sequence that can be used to determine if it's valid in a given
// situation, e.g. if "var()" is present (has_references=true), then the
// sequence is valid for any property [2].
//
// Braces (i.e. {}) are considered to be "positioned" when they appear
// top-level with non-whitespace tokens to the left or the right.
//
// For example:
//
//   foo {}    =>  Positioned
//   {} foo    =>  Positioned
//   { foo }   =>  Not positioned (the {} covers the whole value).
//   foo [{}]  =>  Not positioned (the {} appears within another block).
//
// Token sequences with "positioned" braces are not valid in standard
// properties, even if var()/env() is present in the value [3].
//
// [1] https://drafts.csswg.org/css-syntax-3/#typedef-declaration-value
// [2] https://drafts.csswg.org/css-variables/#using-variables
// [3] https://github.com/w3c/csswg-drafts/issues/9317
bool IsValidRestrictedDeclarationValue(CSSParserTokenRange range,
                                       bool& has_references,
                                       bool& has_positioned_braces,
                                       const ExecutionContext* context) {
  size_t block_stack_size = 0;

  // https://drafts.csswg.org/css-syntax/#component-value
  size_t top_level_component_values = 0;
  bool has_top_level_brace = false;

  while (!range.AtEnd()) {
    if (block_stack_size == 0 && range.Peek().GetType() != kWhitespaceToken) {
      ++top_level_component_values;
      if (range.Peek().GetType() == kLeftBraceToken) {
        has_top_level_brace = true;
      }
    }

    // First check if this is a valid variable reference, then handle the next
    // token accordingly.
    if (range.Peek().GetBlockType() == CSSParserToken::kBlockStart) {
      const CSSParserToken& token = range.Peek();

      // A block may have both var and env references. They can also be nested
      // and used as fallbacks.
      switch (token.FunctionId()) {
        case CSSValueID::kInvalid:
          // Not a built-in function, but it might be a user-defined
          // CSS function (e.g. --foo()).
          if (RuntimeEnabledFeatures::CSSFunctionsEnabled() &&
              token.GetType() == kFunctionToken &&
              CSSVariableParser::IsValidVariableName(token.Value())) {
            has_references = true;
          }
          break;
        case CSSValueID::kVar:
          if (!IsValidVariableReference(range.ConsumeBlock(), context)) {
            return false;  // Invalid reference.
          }
          has_references = true;
          continue;
        case CSSValueID::kEnv:
          if (!IsValidEnvVariableReference(range.ConsumeBlock(), context)) {
            return false;  // Invalid reference.
          }
          has_references = true;
          continue;
        case CSSValueID::kAttr:
          if (!RuntimeEnabledFeatures::CSSAdvancedAttrFunctionEnabled()) {
            break;
          }
          if (!IsValidAttributeReference(range.ConsumeBlock(), context)) {
            return false;  // Invalid reference.
          }
          has_references = true;
          continue;
        default:
          break;
      }
    }

    const CSSParserToken& token = range.Consume();
    if (token.GetBlockType() == CSSParserToken::kBlockStart) {
      ++block_stack_size;
    } else if (token.GetBlockType() == CSSParserToken::kBlockEnd) {
      --block_stack_size;
    } else {
      switch (token.GetType()) {
        case kDelimiterToken: {
          if (token.Delimiter() == '!' && block_stack_size == 0) {
            return false;
          }
          break;
        }
        case kRightParenthesisToken:
        case kRightBraceToken:
        case kRightBracketToken:
        case kBadStringToken:
        case kBadUrlToken:
          return false;
        case kSemicolonToken:
          if (block_stack_size == 0) {
            return false;
          }
          break;
        default:
          break;
      }
    }
  }

  has_positioned_braces =
      has_top_level_brace && (top_level_component_values > 1);

  return true;
}

bool IsValidVariableReference(CSSParserTokenRange range,
                              const ExecutionContext* context) {
  range.ConsumeWhitespace();
  if (!CSSVariableParser::IsValidVariableName(
          range.ConsumeIncludingWhitespace())) {
    return false;
  }
  if (range.AtEnd()) {
    return true;
  }

  if (range.Consume().GetType() != kCommaToken) {
    return false;
  }

  bool has_references = false;
  bool has_positioned_braces = false;
  return IsValidRestrictedDeclarationValue(range, has_references,
                                           has_positioned_braces, context);
}

bool IsValidEnvVariableReference(CSSParserTokenRange range,
                                 const ExecutionContext* context) {
  range.ConsumeWhitespace();
  auto token = range.ConsumeIncludingWhitespace();
  if (token.GetType() != CSSParserTokenType::kIdentToken) {
    return false;
  }
  if (range.AtEnd()) {
    return true;
  }

  if (RuntimeEnabledFeatures::ViewportSegmentsEnabled(context)) {
    // Consume any number of integer values that indicate the indices for a
    // multi-dimensional variable.
    token = range.ConsumeIncludingWhitespace();
    while (token.GetType() == kNumberToken) {
      if (token.GetNumericValueType() != kIntegerValueType) {
        return false;
      }
      if (token.NumericValue() < 0.) {
        return false;
      }
      token = range.ConsumeIncludingWhitespace();
    }

    // If that's all we had (either ident then integers or just the ident) then
    // the env() is valid.
    if (token.GetType() == kEOFToken) {
      return true;
    }
  } else {
    token = range.Consume();
  }

  // Otherwise we need a comma followed by an optional fallback value.
  if (token.GetType() != kCommaToken) {
    return false;
  }

  bool has_references = false;
  bool has_positioned_braces = false;
  return IsValidRestrictedDeclarationValue(range, has_references,
                                           has_positioned_braces, context);
}

// attr() = attr( <attr-name> <attr-type>? , <declaration-value>?)
bool IsValidAttributeReference(CSSParserTokenRange range,
                               const ExecutionContext* context) {
  range.ConsumeWhitespace();
  // Parse <attr-name>.
  auto token = range.ConsumeIncludingWhitespace();
  if (token.GetType() != kIdentToken) {
    return false;
  }
  if (range.AtEnd()) {
    // attr = attr(<attr-name>) is allowed, so return true.
    return true;
  }

  token = range.ConsumeIncludingWhitespace();

  // Parse <attr-type>.
  if (token.GetType() == kIdentToken) {
    if (!CSSAttrType::Parse(token.Value()).IsValid()) {
      return false;
    }
    if (range.AtEnd()) {
      // attr = attr(<attr-name> <attr-type>) is allowed, so return true.
      return true;
    }
  }

  if (range.Consume().GetType() != kCommaToken) {
    return false;
  }
  if (range.AtEnd()) {
    return false;
  }
  bool has_references = false;
  bool has_positioned_braces = false;
  return IsValidRestrictedDeclarationValue(range, has_references,
                                           has_positioned_braces, context);
}

bool IsValidVariable(CSSParserTokenRange range,
                     bool& has_references,
                     bool& has_positioned_braces,
                     const ExecutionContext* context) {
  has_references = false;
  has_positioned_braces = false;
  return IsValidRestrictedDeclarationValue(range, has_references,
                                           has_positioned_braces, context);
}

CSSValue* ParseCSSWideValue(CSSParserTokenRange range) {
  range.ConsumeWhitespace();
  CSSValue* value = css_parsing_utils::ConsumeCSSWideKeyword(range);
  return range.AtEnd() ? value : nullptr;
}

}  // namespace

bool CSSVariableParser::IsValidVariableName(const CSSParserToken& token) {
  if (token.GetType() != kIdentToken) {
    return false;
  }

  return IsValidVariableName(token.Value());
}

bool CSSVariableParser::IsValidVariableName(StringView string) {
  return string.length() >= 3 && string[0] == '-' && string[1] == '-';
}

CSSValue* CSSVariableParser::ParseDeclarationIncludingCSSWide(
    const CSSTokenizedValue& tokenized_value,
    bool is_animation_tainted,
    const CSSParserContext& context) {
  if (CSSValue* css_wide = ParseCSSWideValue(tokenized_value.range)) {
    return css_wide;
  }
  return ParseDeclarationValue(tokenized_value, is_animation_tainted, context);
}

CSSUnparsedDeclarationValue* CSSVariableParser::ParseDeclarationValue(
    const CSSTokenizedValue& tokenized_value,
    bool is_animation_tainted,
    const CSSParserContext& context) {
  bool has_references;
  bool has_positioned_braces_ignored;
  // Note that positioned braces are allowed in custom property declarations.
  if (!IsValidVariable(tokenized_value.range, has_references,
                       has_positioned_braces_ignored,
                       context.GetExecutionContext())) {
    return nullptr;
  }
  if (tokenized_value.text.length() > CSSVariableData::kMaxVariableBytes) {
    return nullptr;
  }

  StringView text = StripTrailingWhitespaceAndComments(tokenized_value.text);
  return MakeGarbageCollected<CSSUnparsedDeclarationValue>(
      CSSVariableData::Create(CSSTokenizedValue{tokenized_value.range, text},
                              is_animation_tainted, has_references),
      &context);
}

static bool ConsumeUnparsedValue(CSSParserTokenStream& stream,
                                 bool restricted_value,
                                 bool comma_ends_declaration,
                                 bool& has_references,
                                 bool& has_font_units,
                                 bool& has_root_font_units,
                                 bool& has_line_height_units,
                                 const ExecutionContext* context);

static bool ConsumeVariableReference(CSSParserTokenStream& stream,
                                     bool& has_references,
                                     bool& has_font_units,
                                     bool& has_root_font_units,
                                     bool& has_line_height_units,
                                     const ExecutionContext* context) {
  CSSParserTokenStream::BlockGuard guard(stream);
  stream.ConsumeWhitespace();
  if (stream.Peek().GetType() != kIdentToken ||
      !CSSVariableParser::IsValidVariableName(
          stream.ConsumeIncludingWhitespace())) {
    return false;
  }
  if (stream.AtEnd()) {
    return true;
  }

  if (stream.Consume().GetType() != kCommaToken) {
    return false;
  }

  // Parse the fallback value.
  if (!ConsumeUnparsedValue(stream, /*restricted_value=*/false,
                            /*comma_ends_declaration=*/false, has_references,
                            has_font_units, has_root_font_units,
                            has_line_height_units, context)) {
    return false;
  }
  return stream.AtEnd();
}

static bool ConsumeEnvVariableReference(CSSParserTokenStream& stream,
                                        bool& has_references,
                                        bool& has_font_units,
                                        bool& has_root_font_units,
                                        bool& has_line_height_units,
                                        const ExecutionContext* context) {
  CSSParserTokenStream::BlockGuard guard(stream);
  stream.ConsumeWhitespace();
  if (stream.Peek().GetType() != kIdentToken) {
    return false;
  }
  CSSParserToken token = stream.ConsumeIncludingWhitespace();
  if (stream.AtEnd()) {
    return true;
  }

  if (RuntimeEnabledFeatures::ViewportSegmentsEnabled(context)) {
    // Consume any number of integer values that indicate the indices for a
    // multi-dimensional variable.
    while (stream.Peek().GetType() == kNumberToken) {
      token = stream.ConsumeIncludingWhitespace();
      if (token.GetNumericValueType() != kIntegerValueType) {
        return false;
      }
      if (token.NumericValue() < 0.) {
        return false;
      }
    }

    // If that's all we had (either ident then integers or just the ident) then
    // the env() is valid.
    if (stream.AtEnd()) {
      return true;
    }
    token = stream.ConsumeIncludingWhitespace();
  } else {
    token = stream.Consume();
  }
  // Otherwise we need a comma followed by an optional fallback value.
  if (token.GetType() != kCommaToken) {
    return false;
  }

  // Parse the fallback value.
  if (!ConsumeUnparsedValue(stream, /*restricted_value=*/false,
                            /*comma_ends_declaration=*/false, has_references,
                            has_font_units, has_root_font_units,
                            has_line_height_units, context)) {
    return false;
  }
  return stream.AtEnd();
}

// attr() = attr( <attr-name> <attr-type>? , <declaration-value>?)
static bool ConsumeAttributeReference(CSSParserTokenStream& stream,
                                      bool& has_references,
                                      bool& has_font_units,
                                      bool& has_root_font_units,
                                      bool& has_line_height_units,
                                      const ExecutionContext* context) {
  CSSParserTokenStream::BlockGuard guard(stream);
  stream.ConsumeWhitespace();
  // Parse <attr-name>.
  auto token = stream.ConsumeIncludingWhitespace();
  if (token.GetType() != kIdentToken) {
    return false;
  }
  if (stream.AtEnd()) {
    // attr = attr(<attr-name>) is allowed, so return true.
    return true;
  }

  token = stream.ConsumeIncludingWhitespace();

  // Parse <attr-type>.
  if (token.GetType() == kIdentToken) {
    if (!CSSAttrType::Parse(token.Value()).IsValid()) {
      return false;
    }
    if (stream.AtEnd()) {
      // attr = attr(<attr-name> <attr-type>) is allowed, so return true.
      return true;
    }
  }

  if (stream.Peek().GetType() != kCommaToken) {
    return false;
  }
  stream.Consume();
  if (stream.AtEnd()) {
    return false;
  }

  // Parse the fallback value.
  if (!ConsumeUnparsedValue(stream, /*restricted_value=*/false,
                            /*comma_ends_declaration=*/false, has_references,
                            has_font_units, has_root_font_units,
                            has_line_height_units, context)) {
    return false;
  }
  return stream.AtEnd();
}

// Utility function for ConsumeUnparsedDeclaration(). Parses until it
// detects some error (such as a stray top-level right-paren; if so,
// returns false) or something that should end a declaration, such as
// a top-level exclamation semicolon (returns true). AtEnd() must be
// checked by the caller even if this returns success, although on
// top-level, it may need to strip !important first.
//
// Called recursively for parsing fallback values.
static bool ConsumeUnparsedValue(CSSParserTokenStream& stream,
                                 bool restricted_value,
                                 bool comma_ends_declaration,
                                 bool& has_references,
                                 bool& has_font_units,
                                 bool& has_root_font_units,
                                 bool& has_line_height_units,
                                 const ExecutionContext* context) {
  size_t block_stack_size = 0;

  // https://drafts.csswg.org/css-syntax/#component-value
  size_t top_level_component_values = 0;
  bool has_top_level_brace = false;
  bool error = false;

  while (true) {
    const CSSParserToken& token = stream.Peek();
    if (token.IsEOF()) {
      break;
    }

    // Save this, since we'll change it below.
    const bool at_top_level = block_stack_size == 0;

    // First check if this is a valid variable reference, then handle the next
    // token accordingly.
    if (token.GetBlockType() == CSSParserToken::kBlockStart) {
      // A block may have both var and env references. They can also be nested
      // and used as fallbacks.
      switch (token.FunctionId()) {
        case CSSValueID::kInvalid:
          // Not a built-in function, but it might be a user-defined
          // CSS function (e.g. --foo()).
          if (RuntimeEnabledFeatures::CSSFunctionsEnabled() &&
              token.GetType() == kFunctionToken &&
              CSSVariableParser::IsValidVariableName(token.Value())) {
            has_references = true;
          }
          break;
        case CSSValueID::kVar:
          if (!ConsumeVariableReference(stream, has_references, has_font_units,
                                        has_root_font_units,
                                        has_line_height_units, context)) {
            error = true;
          }
          has_references = true;
          continue;
        case CSSValueID::kEnv:
          if (!ConsumeEnvVariableReference(stream, has_references,
                                           has_font_units, has_root_font_units,
                                           has_line_height_units, context)) {
            error = true;
          }
          has_references = true;
          continue;
        case CSSValueID::kAttr:
          if (!RuntimeEnabledFeatures::CSSAdvancedAttrFunctionEnabled()) {
            break;
          }
          if (!ConsumeAttributeReference(stream, has_references, has_font_units,
                                         has_root_font_units,
                                         has_line_height_units, context)) {
            error = true;
          }
          has_references = true;
          continue;
        default:
          break;
      }
    }

    if (token.GetBlockType() == CSSParserToken::kBlockStart) {
      ++block_stack_size;
    } else if (token.GetBlockType() == CSSParserToken::kBlockEnd) {
      if (block_stack_size == 0) {
        break;
      }
      --block_stack_size;
    } else {
      switch (token.GetType()) {
        case kDelimiterToken: {
          if (token.Delimiter() == '!' && block_stack_size == 0) {
            return !error;
          }
          break;
        }
        case kRightParenthesisToken:
        case kRightBraceToken:
        case kRightBracketToken:
        case kBadStringToken:
        case kBadUrlToken:
          error = true;
          break;
        case kSemicolonToken:
          if (block_stack_size == 0) {
            return !error;
          }
          break;
        case kCommaToken:
          if (comma_ends_declaration && block_stack_size == 0) {
            return !error;
          }
          break;
        default:
          break;
      }
    }

    if (error && at_top_level) {
      // We cannot safely exit until we are at the top level; this is a waste,
      // but it's not a big problem since we need to fast-forward through error
      // recovery in nearly all cases anyway (the only exception would be when
      // we retry as a nested rule, but nested rules that look like custom
      // property declarations are illegal and cannot happen in legal CSS).
      return false;
    }

    // Now that we know this token wasn't an end-of-value marker,
    // check whether we are violating the rules for restricted values.
    if (restricted_value && at_top_level) {
      ++top_level_component_values;
      if (token.GetType() == kLeftBraceToken) {
        has_top_level_brace = true;
      }
      if (has_top_level_brace && top_level_component_values > 1) {
        return false;
      }
    }

    CSSVariableData::ExtractFeatures(token, has_font_units, has_root_font_units,
                                     has_line_height_units);
    stream.ConsumeRaw();
  }

  return !error;
}

CSSVariableData* CSSVariableParser::ConsumeUnparsedDeclaration(
    CSSParserTokenStream& stream,
    bool allow_important_annotation,
    bool is_animation_tainted,
    bool must_contain_variable_reference,
    bool restricted_value,
    bool comma_ends_declaration,
    bool& important,
    const ExecutionContext* context) {
  // Consume leading whitespace and comments, as required by the spec.
  stream.ConsumeWhitespace();
  stream.EnsureLookAhead();
  wtf_size_t value_start_offset = stream.LookAheadOffset();

  bool has_references = false;
  bool has_font_units = false;
  bool has_root_font_units = false;
  bool has_line_height_units = false;
  if (!ConsumeUnparsedValue(stream, restricted_value, comma_ends_declaration,
                            has_references, has_font_units, has_root_font_units,
                            has_line_height_units, context)) {
    return nullptr;
  }

  if (must_contain_variable_reference && !has_references) {
    return nullptr;
  }

  stream.EnsureLookAhead();
  wtf_size_t value_end_offset = stream.LookAheadOffset();

  important = css_parsing_utils::MaybeConsumeImportant(
      stream, allow_important_annotation);
  if (!stream.AtEnd() &&
      !(comma_ends_declaration && stream.Peek().GetType() == kCommaToken)) {
    return nullptr;
  }

  StringView original_text = stream.StringRangeAt(
      value_start_offset, value_end_offset - value_start_offset);

  if (original_text.length() > CSSVariableData::kMaxVariableBytes) {
    return nullptr;
  }
  original_text =
      CSSVariableParser::StripTrailingWhitespaceAndComments(original_text);

  return CSSVariableData::Create(original_text, is_animation_tainted,
                                 /*needs_variable_resolution=*/has_references,
                                 has_font_units, has_root_font_units,
                                 has_line_height_units);
}

CSSUnparsedDeclarationValue* CSSVariableParser::ParseUniversalSyntaxValue(
    CSSTokenizedValue value,
    const CSSParserContext& context,
    bool is_animation_tainted) {
  bool has_references;
  bool has_positioned_braces_ignored;
  if (!IsValidVariable(value.range, has_references,
                       has_positioned_braces_ignored,
                       context.GetExecutionContext())) {
    return nullptr;
  }
  if (ParseCSSWideValue(value.range)) {
    return nullptr;
  }
  return MakeGarbageCollected<CSSUnparsedDeclarationValue>(
      CSSVariableData::Create(value, is_animation_tainted, has_references),
      &context);
}

StringView CSSVariableParser::StripTrailingWhitespaceAndComments(
    StringView text) {
  // Comments may (unfortunately!) be unfinished, so we can't rely on
  // looking for */; if there's /* anywhere, we'll need to scan through
  // the string from the start. We do a very quick heuristic first
  // to get rid of the most common cases.
  //
  // TODO(sesse): In the cases where we've tokenized the string before
  // (i.e. not CSSOM, where we just get a string), we know we can't
  // have unfinished comments, so consider piping that knowledge all
  // the way through here.
  if (text.Is8Bit() && !base::Contains(text.Span8(), '/')) {
    // No comments, so we can strip whitespace only.
    while (!text.empty() && IsHTMLSpace(text[text.length() - 1])) {
      text = StringView(text, 0, text.length() - 1);
    }
    return text;
  }

  wtf_size_t string_len = 0;
  bool in_comment = false;
  for (wtf_size_t i = 0; i < text.length(); ++i) {
    if (in_comment) {
      // See if we can end this comment.
      if (text[i] == '*' && i + 1 < text.length() && text[i + 1] == '/') {
        ++i;
        in_comment = false;
      }
    } else {
      // See if we must start a comment.
      if (text[i] == '/' && i + 1 < text.length() && text[i + 1] == '*') {
        ++i;
        in_comment = true;
      } else if (!IsHTMLSpace(text[i])) {
        // A non-space outside a comment, so the string
        // must go at least to here.
        string_len = i + 1;
      }
    }
  }

  StringView ret = StringView(text, 0, string_len);

  // Leading whitespace should already have been stripped.
  // (This test needs to be after we stripped trailing spaces,
  // or we could look at trailing space believing it was leading.)
  DCHECK(ret.empty() || !IsHTMLSpace(ret[0]));

  return ret;
}

}  // namespace blink
