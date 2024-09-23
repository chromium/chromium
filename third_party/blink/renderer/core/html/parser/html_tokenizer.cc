/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"

#include "third_party/blink/renderer/core/html/parser/html_entity_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/html_tree_builder.h"
#include "third_party/blink/renderer/core/html/parser/markup_tokenizer_inlines.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/html_tokenizer_names.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

// clang-format off
#define INT_0_TO_127_LIST(V)                                                    \
V(0),   V(1),   V(2),   V(3),   V(4),   V(5),   V(6),   V(7),   V(8),   V(9),   \
V(10),  V(11),  V(12),  V(13),  V(14),  V(15),  V(16),  V(17),  V(18),  V(19),  \
V(20),  V(21),  V(22),  V(23),  V(24),  V(25),  V(26),  V(27),  V(28),  V(29),  \
V(30),  V(31),  V(32),  V(33),  V(34),  V(35),  V(36),  V(37),  V(38),  V(39),  \
V(40),  V(41),  V(42),  V(43),  V(44),  V(45),  V(46),  V(47),  V(48),  V(49),  \
V(50),  V(51),  V(52),  V(53),  V(54),  V(55),  V(56),  V(57),  V(58),  V(59),  \
V(60),  V(61),  V(62),  V(63),  V(64),  V(65),  V(66),  V(67),  V(68),  V(69),  \
V(70),  V(71),  V(72),  V(73),  V(74),  V(75),  V(76),  V(77),  V(78),  V(79),  \
V(80),  V(81),  V(82),  V(83),  V(84),  V(85),  V(86),  V(87),  V(88),  V(89),  \
V(90),  V(91),  V(92),  V(93),  V(94),  V(95),  V(96),  V(97),  V(98),  V(99),  \
V(100), V(101), V(102), V(103), V(104), V(105), V(106), V(107), V(108), V(109), \
V(110), V(111), V(112), V(113), V(114), V(115), V(116), V(117), V(118), V(119), \
V(120), V(121), V(122), V(123), V(124), V(125), V(126), V(127),
// clang-format on

// Character flags for fast paths.
enum class ScanFlags : uint16_t {
  // Base flags
  kNullCharacter = 1 << 0,
  kNewlineOrCarriageReturn = 1 << 1,
  kWhitespaceNotNewline = 1 << 2,
  kAmpersand = 1 << 3,
  kOpenTag = 1 << 4,
  kSlashAndCloseTag = 1 << 5,
  kEqual = 1 << 6,
  kQuotes = 1 << 7,
  kOpenBrace = 1 << 8,
  // Compound flags
  kWhitespace = kWhitespaceNotNewline | kNewlineOrCarriageReturn,
  kCharacterTokenSpecial = kNullCharacter | kNewlineOrCarriageReturn |
                           kAmpersand | kOpenTag | kOpenBrace,
  kNullOrNewline = kNullCharacter | kNewlineOrCarriageReturn,
  kRCDATASpecial = kNullCharacter | kAmpersand | kOpenTag,
  kTagNameSpecial = kWhitespace | kSlashAndCloseTag | kNullCharacter,
  kAttributeNameSpecial = kWhitespace | kSlashAndCloseTag | kNullCharacter |
                          kEqual | kOpenTag | kQuotes,
};

static constexpr uint16_t CreateScanFlags(UChar cc) {
#define SCAN_FLAG(flag) static_cast<uint16_t>(ScanFlags::flag)
  DCHECK(!(cc & ~0x7F));  // IsASCII
  uint16_t scan_flag = 0;
  if (cc == '\0') {
    scan_flag = SCAN_FLAG(kNullCharacter);
  } else if (cc == '\n' || cc == '\r') {
    scan_flag = SCAN_FLAG(kNewlineOrCarriageReturn);
  } else if (cc == ' ' || cc == '\x09' || cc == '\x0C') {
    scan_flag = SCAN_FLAG(kWhitespaceNotNewline);
  } else if (cc == '&') {
    scan_flag = SCAN_FLAG(kAmpersand);
  } else if (cc == '<') {
    scan_flag = SCAN_FLAG(kOpenTag);
  } else if (cc == '/' || cc == '>') {
    scan_flag = SCAN_FLAG(kSlashAndCloseTag);
  } else if (cc == '=') {
    scan_flag = SCAN_FLAG(kEqual);
  } else if (cc == '"' || cc == '\'') {
    scan_flag = SCAN_FLAG(kQuotes);
  } else if (cc == '{') {
    scan_flag = SCAN_FLAG(kOpenBrace);
  }
  return scan_flag;
#undef SCAN_FLAG
}

// DOM Part marker strings. Eventually move these to html_tokenizer_names.
#define kChildNodePartStartMarker "{{#}}"
#define kChildNodePartEndMarker "{{/}}"
#define kNodePartMarker "{{}}"
#define kAttributePartMarker "{{}}"

// Table of precomputed scan flags for the first 128 ASCII characters.
static constexpr const uint16_t character_scan_flags_[128] = {
    INT_0_TO_127_LIST(CreateScanFlags)};

static inline UChar ToLowerCase(UChar cc) {
  DCHECK(IsASCIIAlpha(cc));
  return cc | 0x20;
}

static inline bool CheckScanFlag(UChar cc, ScanFlags flag) {
  return IsASCII(cc) &&
         (character_scan_flags_[cc] & static_cast<uint16_t>(flag));
}

static inline UChar ToLowerCaseIfAlpha(UChar cc) {
  return cc | (IsASCIIUpper(cc) ? 0x20 : 0);
}

static inline bool VectorEqualsString(const LCharLiteralBuffer<32>& vector,
                                      const String& string) {
  if (vector.size() != string.length())
    return false;

  if (!string.length())
    return true;

  return Equal(string.Impl(), vector.data(), vector.size());
}

#define HTML_BEGIN_STATE(stateName) BEGIN_STATE(HTMLTokenizer, stateName)
#define HTML_BEGIN_STATE_NOLABEL(stateName) \
  BEGIN_STATE_NOLABEL(HTMLTokenizer, stateName)
#define HTML_RECONSUME_IN(stateName) RECONSUME_IN(HTMLTokenizer, stateName)
#define HTML_ADVANCE_TO(stateName) ADVANCE_TO(HTMLTokenizer, stateName)
#define HTML_ADVANCE_PAST_NON_NEWLINE_TO(stateName) \
  ADVANCE_PAST_NON_NEWLINE_TO(HTMLTokenizer, stateName)
#define HTML_CONSUME(stateName) CONSUME(HTMLTokenizer, stateName)
#define HTML_CONSUME_NON_NEWLINE(stateName) \
  CONSUME_NON_NEWLINE(HTMLTokenizer, stateName)
#define HTML_SWITCH_TO(stateName) SWITCH_TO(HTMLTokenizer, stateName)

HTMLTokenizer::HTMLTokenizer(const HTMLParserOptions& options)
    : track_attributes_ranges_(options.track_attributes_ranges),
      input_stream_preprocessor_(this),
      options_(options) {
  Reset();
}

HTMLTokenizer::~HTMLTokenizer() = default;

void HTMLTokenizer::Reset() {
  token_.Clear();
  state_ = HTMLTokenizer::kDataState;
  force_null_character_replacement_ = false;
  should_allow_cdata_ = false;
  additional_allowed_character_ = '\0';
}

inline bool HTMLTokenizer::ProcessEntity(SegmentedString& source) {
  bool not_enough_characters = false;
  DecodedHTMLEntity decoded_entity;
  bool success =
      ConsumeHTMLEntity(source, decoded_entity, not_enough_characters);
  if (not_enough_characters)
    return false;
  if (!success) {
    DCHECK(decoded_entity.IsEmpty());
    BufferCharacter('&');
  } else {
    for (unsigned i = 0; i < decoded_entity.length; ++i)
      BufferCharacter(decoded_entity.data[i]);
  }
  return true;
}

bool HTMLTokenizer::FlushBufferedEndTag(SegmentedString& source,
                                        bool current_char_may_be_newline) {
  DCHECK(token_.GetType() == HTMLToken::kCharacter ||
         token_.GetType() == HTMLToken::kUninitialized);
  if (current_char_may_be_newline)
    source.AdvanceAndUpdateLineNumber();
  else
    source.AdvancePastNonNewline();
  if (token_.GetType() == HTMLToken::kCharacter)
    return true;
  token_.BeginEndTag(buffered_end_tag_name_);
  buffered_end_tag_name_.clear();
  appropriate_end_tag_name_.clear();
  temporary_buffer_.clear();
  return false;
}

#define FLUSH_AND_ADVANCE_TO(stateName, current_char_may_be_newline)      \
  do {                                                                    \
    state_ = HTMLTokenizer::stateName;                                    \
    if (FlushBufferedEndTag(source, current_char_may_be_newline))         \
      return true;                                                        \
    if (source.IsEmpty() || !input_stream_preprocessor_.Peek(source, cc)) \
      return HaveBufferedCharacterToken();                                \
    goto stateName;                                                       \
  } while (false)

#define FLUSH_AND_ADVANCE_TO_NO_NEWLINE(stateName) \
  FLUSH_AND_ADVANCE_TO(stateName, /* current_char_may_be_newline */ false)

#define FLUSH_AND_ADVANCE_TO_MAY_CONTAIN_NEWLINE(stateName) \
  FLUSH_AND_ADVANCE_TO(stateName, /* current_char_may_be_newline */ true)

#define ADVANCE_PAST_MULTIPLE_NO_NEWLINE(len, newState)                 \
  {                                                                     \
    DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());               \
    for (unsigned i = 1; i < (len); ++i) {                              \
      bool success =                                                    \
          input_stream_preprocessor_.AdvancePastNonNewline(source, cc); \
      DCHECK(success);                                                  \
    }                                                                   \
    if (state_ == HTMLTokenizer::newState) {                            \
      HTML_CONSUME(newState);                                           \
    } else {                                                            \
      HTML_SWITCH_TO(newState);                                         \
    }                                                                   \
  }

bool HTMLTokenizer::FlushEmitAndResumeInDataState(SegmentedString& source) {
  state_ = HTMLTokenizer::kDataState;
  FlushBufferedEndTag(source, /* current_char_may_be_newline */ false);
  return true;
}

HTMLToken* HTMLTokenizer::NextToken(SegmentedString& source) {
#if DCHECK_IS_ON()
  DCHECK(!token_should_be_in_uninitialized_state_ || token_.IsUninitialized());
  DCHECK(!token_should_be_in_uninitialized_state_ ||
         attributes_ranges_.attributes().empty());
#endif
  const bool completed_token = NextTokenImpl(source);
#if DCHECK_IS_ON()
  // If the token was completed, then the caller is expected to clear it
  // (putting it into the uninitialized state) before NextToken() gets called
  // again.
  token_should_be_in_uninitialized_state_ = completed_token;
#endif
  return completed_token ? &token_ : nullptr;
}

bool HTMLTokenizer::NextTokenImpl(SegmentedString& source) {
  if (!buffered_end_tag_name_.IsEmpty() && !IsEndTagBufferingState(state_)) {
    // FIXME: This should call flushBufferedEndTag().
    // We started an end tag during our last iteration.
    token_.BeginEndTag(buffered_end_tag_name_);
    buffered_end_tag_name_.clear();
    appropriate_end_tag_name_.clear();
    temporary_buffer_.clear();
    if (state_ == HTMLTokenizer::kDataState) {
      // We're back in the data state, so we must be done with the tag.
      return true;
    }
  }

  UChar cc;
  if (source.IsEmpty() || !input_stream_preprocessor_.Peek(source, cc))
    return HaveBufferedCharacterToken();

  // Source: http://www.whatwg.org/specs/web-apps/current-work/#tokenisation0
  switch (state_) {
    HTML_BEGIN_STATE(kDataState) {
      if (cc == '&')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kCharacterReferenceInDataState);
      else if (cc == '<') {
        if (HaveBufferedCharacterToken()) {
          // We have a bunch of character tokens queued up that we
          // are emitting lazily here.
          return true;
        }
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kTagOpenState);
      } else if (cc == kEndOfFileMarker)
        return EmitEndOfFile(source);
      else {
        return EmitData(source, cc);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE_NOLABEL(kChildNodePartStartState) {
      DCHECK_EQ(source.LookAhead(kChildNodePartStartMarker),
                SegmentedString::kDidMatch);
      AdvanceStringAndASSERT(source, kChildNodePartStartMarker);
      token_.BeginDOMPart(DOMPartTokenType::kChildNodePartStart);
      // Emit the DOM Part token and then return to the DATA state.
      state_ = kDataState;
      return true;
    }
    END_STATE()

    HTML_BEGIN_STATE_NOLABEL(kChildNodePartEndState) {
      DCHECK_EQ(source.LookAhead(kChildNodePartEndMarker),
                SegmentedString::kDidMatch);
      AdvanceStringAndASSERT(source, kChildNodePartEndMarker);
      token_.BeginDOMPart(DOMPartTokenType::kChildNodePartEnd);
      // Emit the DOM Part token and then return to the DATA state.
      state_ = kDataState;
      return true;
    }
    END_STATE()

    HTML_BEGIN_STATE(kCharacterReferenceInDataState) {
      if (!ProcessEntity(source))
        return HaveBufferedCharacterToken();
      HTML_SWITCH_TO(kDataState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kRCDATAState) {
      while (!CheckScanFlag(cc, ScanFlags::kRCDATASpecial)) {
        BufferCharacter(cc);
        if (!input_stream_preprocessor_.Advance(source, cc))
          return HaveBufferedCharacterToken();
      }
      if (cc == '&')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kCharacterReferenceInRCDATAState);
      else if (cc == '<')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kRCDATALessThanSignState);
      else if (cc == kEndOfFileMarker)
        return EmitEndOfFile(source);
      else
        NOTREACHED_IN_MIGRATION();
    }
    END_STATE()

    HTML_BEGIN_STATE(kCharacterReferenceInRCDATAState) {
      if (!ProcessEntity(source))
        return HaveBufferedCharacterToken();
      HTML_SWITCH_TO(kRCDATAState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kRAWTEXTState) {
      if (cc == '<')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kRAWTEXTLessThanSignState);
      else if (cc == kEndOfFileMarker)
        return EmitEndOfFile(source);
      else {
        BufferCharacter(cc);
        HTML_CONSUME(kRAWTEXTState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataState) {
      if (cc == '<')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataLessThanSignState);
      else if (cc == kEndOfFileMarker)
        return EmitEndOfFile(source);
      else {
        BufferCharacter(cc);
        HTML_CONSUME(kScriptDataState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE_NOLABEL(kPLAINTEXTState) {
      if (cc == kEndOfFileMarker)
        return EmitEndOfFile(source);
      return EmitPLAINTEXT(source, cc);
    }
    END_STATE()

    HTML_BEGIN_STATE(kTagOpenState) {
      if (IsASCIIAlpha(cc)) {
        token_.BeginStartTag(ToLowerCase(cc));
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kTagNameState);
      } else if (cc == '!') {
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kMarkupDeclarationOpenState);
      } else if (cc == '/') {
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kEndTagOpenState);
      } else if (cc == '?') {
        ParseError();
        // The spec consumes the current character before switching
        // to the bogus comment state, but it's easier to implement
        // if we reconsume the current character.
        HTML_RECONSUME_IN(kBogusCommentState);
      } else {
        ParseError();
        BufferCharacter('<');
        HTML_RECONSUME_IN(kDataState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kEndTagOpenState) {
      if (IsASCIIAlpha(cc)) {
        token_.BeginEndTag(static_cast<LChar>(ToLowerCase(cc)));
        appropriate_end_tag_name_.clear();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kTagNameState);
      } else if (cc == '>') {
        ParseError();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        BufferCharacter('<');
        BufferCharacter('/');
        HTML_RECONSUME_IN(kDataState);
      } else {
        ParseError();
        HTML_RECONSUME_IN(kBogusCommentState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kTagNameState) {
      while (!CheckScanFlag(cc, ScanFlags::kTagNameSpecial)) {
        token_.AppendToName(ToLowerCaseIfAlpha(cc));
        if (!input_stream_preprocessor_.AdvancePastNonNewline(source, cc))
          return HaveBufferedCharacterToken();
      }
      if (cc == '/') {
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kSelfClosingStartTagState);
      } else if (cc == '>') {
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else {
        DCHECK(IsTokenizerWhitespace(cc));
        HTML_ADVANCE_TO(kBeforeAttributeNameState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kRCDATALessThanSignState) {
      if (cc == '/') {
        temporary_buffer_.clear();
        DCHECK(buffered_end_tag_name_.IsEmpty());
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kRCDATAEndTagOpenState);
      } else {
        BufferCharacter('<');
        HTML_RECONSUME_IN(kRCDATAState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kRCDATAEndTagOpenState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.AddChar(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kRCDATAEndTagNameState);
      } else {
        BufferCharacter('<');
        BufferCharacter('/');
        HTML_RECONSUME_IN(kRCDATAState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kRCDATAEndTagNameState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.AddChar(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_CONSUME_NON_NEWLINE(kRCDATAEndTagNameState);
      } else {
        if (IsTokenizerWhitespace(cc)) {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.AddChar(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO_MAY_CONTAIN_NEWLINE(kBeforeAttributeNameState);
          }
        } else if (cc == '/') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.AddChar(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO_NO_NEWLINE(kSelfClosingStartTagState);
          }
        } else if (cc == '>') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.AddChar(static_cast<LChar>(cc));
            return FlushEmitAndResumeInDataState(source);
          }
        }
        BufferCharacter('<');
        BufferCharacter('/');
        token_.AppendToCharacter(temporary_buffer_);
        buffered_end_tag_name_.clear();
        temporary_buffer_.clear();
        HTML_RECONSUME_IN(kRCDATAState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kRAWTEXTLessThanSignState) {
      if (cc == '/') {
        temporary_buffer_.clear();
        DCHECK(buffered_end_tag_name_.IsEmpty());
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kRAWTEXTEndTagOpenState);
      } else {
        BufferCharacter('<');
        HTML_RECONSUME_IN(kRAWTEXTState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kRAWTEXTEndTagOpenState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.AddChar(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kRAWTEXTEndTagNameState);
      } else {
        BufferCharacter('<');
        BufferCharacter('/');
        HTML_RECONSUME_IN(kRAWTEXTState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kRAWTEXTEndTagNameState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.AddChar(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_CONSUME_NON_NEWLINE(kRAWTEXTEndTagNameState);
      } else {
        if (IsTokenizerWhitespace(cc)) {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.AddChar(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO_MAY_CONTAIN_NEWLINE(kBeforeAttributeNameState);
          }
        } else if (cc == '/') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.AddChar(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO_NO_NEWLINE(kSelfClosingStartTagState);
          }
        } else if (cc == '>') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.AddChar(static_cast<LChar>(cc));
            return FlushEmitAndResumeInDataState(source);
          }
        }
        BufferCharacter('<');
        BufferCharacter('/');
        token_.AppendToCharacter(temporary_buffer_);
        buffered_end_tag_name_.clear();
        temporary_buffer_.clear();
        HTML_RECONSUME_IN(kRAWTEXTState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataLessThanSignState) {
      if (cc == '/') {
        temporary_buffer_.clear();
        DCHECK(buffered_end_tag_name_.IsEmpty());
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataEndTagOpenState);
      } else if (cc == '!') {
        BufferCharacter('<');
        BufferCharacter('!');
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataEscapeStartState);
      } else {
        BufferCharacter('<');
        HTML_RECONSUME_IN(kScriptDataState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEndTagOpenState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.AddChar(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataEndTagNameState);
      } else {
        BufferCharacter('<');
        BufferCharacter('/');
        HTML_RECONSUME_IN(kScriptDataState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEndTagNameState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.AddChar(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_CONSUME_NON_NEWLINE(kScriptDataEndTagNameState);
      } else {
        if (IsTokenizerWhitespace(cc)) {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.AddChar(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO_MAY_CONTAIN_NEWLINE(kBeforeAttributeNameState);
          }
        } else if (cc == '/') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.AddChar(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO_NO_NEWLINE(kSelfClosingStartTagState);
          }
        } else if (cc == '>') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.AddChar(static_cast<LChar>(cc));
            return FlushEmitAndResumeInDataState(source);
          }
        }
        BufferCharacter('<');
        BufferCharacter('/');
        token_.AppendToCharacter(temporary_buffer_);
        buffered_end_tag_name_.clear();
        temporary_buffer_.clear();
        HTML_RECONSUME_IN(kScriptDataState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEscapeStartState) {
      if (cc == '-') {
        BufferCharacter(cc);
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataEscapeStartDashState);
      } else
        HTML_RECONSUME_IN(kScriptDataState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEscapeStartDashState) {
      if (cc == '-') {
        BufferCharacter(cc);
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataEscapedDashDashState);
      } else
        HTML_RECONSUME_IN(kScriptDataState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEscapedState) {
      if (cc == '-') {
        BufferCharacter(cc);
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataEscapedDashState);
      } else if (cc == '<')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataEscapedLessThanSignState);
      else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else {
        BufferCharacter(cc);
        HTML_CONSUME(kScriptDataEscapedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEscapedDashState) {
      if (cc == '-') {
        BufferCharacter(cc);
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataEscapedDashDashState);
      } else if (cc == '<')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataEscapedLessThanSignState);
      else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else {
        BufferCharacter(cc);
        HTML_ADVANCE_TO(kScriptDataEscapedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEscapedDashDashState) {
      if (cc == '-') {
        BufferCharacter(cc);
        HTML_CONSUME_NON_NEWLINE(kScriptDataEscapedDashDashState);
      } else if (cc == '<')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataEscapedLessThanSignState);
      else if (cc == '>') {
        BufferCharacter(cc);
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else {
        BufferCharacter(cc);
        HTML_ADVANCE_TO(kScriptDataEscapedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEscapedLessThanSignState) {
      if (cc == '/') {
        temporary_buffer_.clear();
        DCHECK(buffered_end_tag_name_.IsEmpty());
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataEscapedEndTagOpenState);
      } else if (IsASCIIAlpha(cc)) {
        BufferCharacter('<');
        BufferCharacter(cc);
        temporary_buffer_.clear();
        temporary_buffer_.AddChar(static_cast<LChar>(ToLowerCase(cc)));
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataDoubleEscapeStartState);
      } else {
        BufferCharacter('<');
        HTML_RECONSUME_IN(kScriptDataEscapedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEscapedEndTagOpenState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.AddChar(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataEscapedEndTagNameState);
      } else {
        BufferCharacter('<');
        BufferCharacter('/');
        HTML_RECONSUME_IN(kScriptDataEscapedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEscapedEndTagNameState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.AddChar(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_CONSUME_NON_NEWLINE(kScriptDataEscapedEndTagNameState);
      } else {
        if (IsTokenizerWhitespace(cc)) {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.AddChar(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO_MAY_CONTAIN_NEWLINE(kBeforeAttributeNameState);
          }
        } else if (cc == '/') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.AddChar(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO_NO_NEWLINE(kSelfClosingStartTagState);
          }
        } else if (cc == '>') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.AddChar(static_cast<LChar>(cc));
            return FlushEmitAndResumeInDataState(source);
          }
        }
        BufferCharacter('<');
        BufferCharacter('/');
        token_.AppendToCharacter(temporary_buffer_);
        buffered_end_tag_name_.clear();
        temporary_buffer_.clear();
        HTML_RECONSUME_IN(kScriptDataEscapedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataDoubleEscapeStartState) {
      if (IsTokenizerWhitespace(cc) || cc == '/' || cc == '>') {
        BufferCharacter(cc);
        if (TemporaryBufferIs(html_names::kScriptTag.LocalName()))
          HTML_ADVANCE_TO(kScriptDataDoubleEscapedState);
        else
          HTML_ADVANCE_TO(kScriptDataEscapedState);
      } else if (IsASCIIAlpha(cc)) {
        BufferCharacter(cc);
        temporary_buffer_.AddChar(static_cast<LChar>(ToLowerCase(cc)));
        HTML_CONSUME_NON_NEWLINE(kScriptDataDoubleEscapeStartState);
      } else
        HTML_RECONSUME_IN(kScriptDataEscapedState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataDoubleEscapedState) {
      if (cc == '-') {
        BufferCharacter(cc);
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataDoubleEscapedDashState);
      } else if (cc == '<') {
        BufferCharacter(cc);
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kScriptDataDoubleEscapedLessThanSignState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else {
        BufferCharacter(cc);
        HTML_CONSUME(kScriptDataDoubleEscapedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataDoubleEscapedDashState) {
      if (cc == '-') {
        BufferCharacter(cc);
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataDoubleEscapedDashDashState);
      } else if (cc == '<') {
        BufferCharacter(cc);
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kScriptDataDoubleEscapedLessThanSignState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else {
        BufferCharacter(cc);
        HTML_ADVANCE_TO(kScriptDataDoubleEscapedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataDoubleEscapedDashDashState) {
      if (cc == '-') {
        BufferCharacter(cc);
        HTML_CONSUME_NON_NEWLINE(kScriptDataDoubleEscapedDashDashState);
      } else if (cc == '<') {
        BufferCharacter(cc);
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kScriptDataDoubleEscapedLessThanSignState);
      } else if (cc == '>') {
        BufferCharacter(cc);
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else {
        BufferCharacter(cc);
        HTML_ADVANCE_TO(kScriptDataDoubleEscapedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataDoubleEscapedLessThanSignState) {
      if (cc == '/') {
        BufferCharacter(cc);
        temporary_buffer_.clear();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kScriptDataDoubleEscapeEndState);
      } else
        HTML_RECONSUME_IN(kScriptDataDoubleEscapedState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataDoubleEscapeEndState) {
      if (IsTokenizerWhitespace(cc) || cc == '/' || cc == '>') {
        BufferCharacter(cc);
        if (TemporaryBufferIs(html_names::kScriptTag.LocalName()))
          HTML_ADVANCE_TO(kScriptDataEscapedState);
        else
          HTML_ADVANCE_TO(kScriptDataDoubleEscapedState);
      } else if (IsASCIIAlpha(cc)) {
        BufferCharacter(cc);
        temporary_buffer_.AddChar(static_cast<LChar>(ToLowerCase(cc)));
        HTML_CONSUME_NON_NEWLINE(kScriptDataDoubleEscapeEndState);
      } else
        HTML_RECONSUME_IN(kScriptDataDoubleEscapedState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kBeforeAttributeNameState) {
      if (!SkipWhitespaces(source, cc))
        return HaveBufferedCharacterToken();
      if (cc == '/') {
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kSelfClosingStartTagState);
      } else if (cc == '>') {
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else if (cc == '{' && ShouldAllowDOMParts() &&
                 source.LookAhead(kNodePartMarker) ==
                     SegmentedString::kDidMatch) {
        static_assert(kNodePartMarker[0] == '{');
        token_.SetNeedsNodePart();
        // Need to skip ahead here so we don't get {{}} as an attribute.
        ADVANCE_PAST_MULTIPLE_NO_NEWLINE(sizeof(kNodePartMarker) - 1,
                                         kBeforeAttributeNameState);
      } else if (cc == '"' || cc == '\'' || cc == '<' || cc == '=') {
        ParseError();
      }
      token_.AddNewAttribute(ToLowerCaseIfAlpha(cc));
      if (track_attributes_ranges_) {
        attributes_ranges_.AddAttribute(source.NumberOfCharactersConsumed());
      }
      HTML_ADVANCE_PAST_NON_NEWLINE_TO(kAttributeNameState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kAttributeNameState) {
      while (!CheckScanFlag(cc, ScanFlags::kAttributeNameSpecial)) {
        token_.AppendToAttributeName(ToLowerCaseIfAlpha(cc));
        if (!input_stream_preprocessor_.AdvancePastNonNewline(source, cc))
          return HaveBufferedCharacterToken();
      }
      if (IsTokenizerWhitespace(cc)) {
        if (track_attributes_ranges_) {
          attributes_ranges_.EndAttributeName(
              source.NumberOfCharactersConsumed());
        }
        HTML_ADVANCE_TO(kAfterAttributeNameState);
      } else if (cc == '/') {
        if (track_attributes_ranges_) {
          attributes_ranges_.EndAttributeName(
              source.NumberOfCharactersConsumed());
        }
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kSelfClosingStartTagState);
      } else if (cc == '=') {
        if (track_attributes_ranges_) {
          attributes_ranges_.EndAttributeName(
              source.NumberOfCharactersConsumed());
        }
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kBeforeAttributeValueState);
      } else if (cc == '>') {
        if (track_attributes_ranges_) {
          attributes_ranges_.EndAttributeName(
              source.NumberOfCharactersConsumed());
        }
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        if (track_attributes_ranges_) {
          attributes_ranges_.EndAttributeName(
              source.NumberOfCharactersConsumed());
        }
        HTML_RECONSUME_IN(kDataState);
      } else {
        DCHECK(cc == '"' || cc == '\'' || cc == '<' || cc == '=');
        ParseError();
        token_.AppendToAttributeName(ToLowerCaseIfAlpha(cc));
        HTML_CONSUME_NON_NEWLINE(kAttributeNameState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAfterAttributeNameState) {
      if (!SkipWhitespaces(source, cc))
        return HaveBufferedCharacterToken();
      if (cc == '/') {
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kSelfClosingStartTagState);
      } else if (cc == '=') {
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kBeforeAttributeValueState);
      } else if (cc == '>') {
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else if (cc == '{' && ShouldAllowDOMParts() &&
                 source.LookAhead(kNodePartMarker) ==
                     SegmentedString::kDidMatch) {
        token_.SetNeedsNodePart();
        // Need to skip ahead here so we don't get {{}} as an attribute.
        ADVANCE_PAST_MULTIPLE_NO_NEWLINE(sizeof(kNodePartMarker) - 1,
                                         kAfterAttributeNameState);
      } else if (cc == '"' || cc == '\'' || cc == '<') {
        ParseError();
      }
      token_.AddNewAttribute(ToLowerCaseIfAlpha(cc));
      if (track_attributes_ranges_) {
        attributes_ranges_.AddAttribute(source.NumberOfCharactersConsumed());
      }
      HTML_ADVANCE_PAST_NON_NEWLINE_TO(kAttributeNameState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kBeforeAttributeValueState) {
      if (IsTokenizerWhitespace(cc))
        HTML_CONSUME(kBeforeAttributeValueState);
      else if (cc == '"') {
        if (track_attributes_ranges_) {
          attributes_ranges_.BeginAttributeValue(
              source.NumberOfCharactersConsumed() + 1);
        }
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kAttributeValueDoubleQuotedState);
      } else if (cc == '&') {
        if (track_attributes_ranges_) {
          attributes_ranges_.BeginAttributeValue(
              source.NumberOfCharactersConsumed());
        }
        HTML_RECONSUME_IN(kAttributeValueUnquotedState);
      } else if (cc == '\'') {
        if (track_attributes_ranges_) {
          attributes_ranges_.BeginAttributeValue(
              source.NumberOfCharactersConsumed() + 1);
        }
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kAttributeValueSingleQuotedState);
      } else if (cc == '>') {
        ParseError();
        return EmitAndResumeInDataState(source);

      } else if (cc == '{' && ShouldAllowDOMParts() &&
                 source.LookAhead(kAttributePartMarker) ==
                     SegmentedString::kDidMatch) {
        static_assert(kAttributePartMarker[0] == '{');
        token_.SetNeedsAttributePart();
        if (track_attributes_ranges_) {
          attributes_ranges_.EndAttributeValue(
              source.NumberOfCharactersConsumed());
        }
        // Skip ahead so we don't get {{}} in the attribute value.
        ADVANCE_PAST_MULTIPLE_NO_NEWLINE(sizeof(kAttributePartMarker) - 1,
                                         kBeforeAttributeNameState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else {
        if (cc == '<' || cc == '=' || cc == '`')
          ParseError();
        if (track_attributes_ranges_) {
          attributes_ranges_.BeginAttributeValue(
              source.NumberOfCharactersConsumed());
        }
        token_.AppendToAttributeValue(cc);
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kAttributeValueUnquotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAttributeValueDoubleQuotedState) {
      if (cc == '"') {
        if (track_attributes_ranges_) {
          attributes_ranges_.EndAttributeValue(
              source.NumberOfCharactersConsumed());
        }
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kAfterAttributeValueQuotedState);
      } else if (cc == '&') {
        additional_allowed_character_ = '"';
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kCharacterReferenceInAttributeValueState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        if (track_attributes_ranges_) {
          attributes_ranges_.EndAttributeValue(
              source.NumberOfCharactersConsumed());
        }
        HTML_RECONSUME_IN(kDataState);
      } else {
        token_.AppendToAttributeValue(cc);
        HTML_CONSUME(kAttributeValueDoubleQuotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAttributeValueSingleQuotedState) {
      if (cc == '\'') {
        if (track_attributes_ranges_) {
          attributes_ranges_.EndAttributeValue(
              source.NumberOfCharactersConsumed());
        }
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kAfterAttributeValueQuotedState);
      } else if (cc == '&') {
        additional_allowed_character_ = '\'';
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kCharacterReferenceInAttributeValueState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        if (track_attributes_ranges_) {
          attributes_ranges_.EndAttributeValue(
              source.NumberOfCharactersConsumed());
        }
        HTML_RECONSUME_IN(kDataState);
      } else {
        token_.AppendToAttributeValue(cc);
        HTML_CONSUME(kAttributeValueSingleQuotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAttributeValueUnquotedState) {
      if (IsTokenizerWhitespace(cc)) {
        if (track_attributes_ranges_) {
          attributes_ranges_.EndAttributeValue(
              source.NumberOfCharactersConsumed());
        }
        HTML_ADVANCE_TO(kBeforeAttributeNameState);
      } else if (cc == '&') {
        additional_allowed_character_ = '>';
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kCharacterReferenceInAttributeValueState);
      } else if (cc == '>') {
        if (track_attributes_ranges_) {
          attributes_ranges_.EndAttributeValue(
              source.NumberOfCharactersConsumed());
        }
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        if (track_attributes_ranges_) {
          attributes_ranges_.EndAttributeValue(
              source.NumberOfCharactersConsumed());
        }
        HTML_RECONSUME_IN(kDataState);
      } else {
        if (cc == '"' || cc == '\'' || cc == '<' || cc == '=' || cc == '`')
          ParseError();
        token_.AppendToAttributeValue(cc);
        HTML_CONSUME_NON_NEWLINE(kAttributeValueUnquotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kCharacterReferenceInAttributeValueState) {
      bool not_enough_characters = false;
      DecodedHTMLEntity decoded_entity;
      bool success =
          ConsumeHTMLEntity(source, decoded_entity, not_enough_characters,
                            additional_allowed_character_);
      if (not_enough_characters)
        return HaveBufferedCharacterToken();
      if (!success) {
        DCHECK(decoded_entity.IsEmpty());
        token_.AppendToAttributeValue('&');
      } else {
        for (unsigned i = 0; i < decoded_entity.length; ++i)
          token_.AppendToAttributeValue(decoded_entity.data[i]);
      }
      // We're supposed to switch back to the attribute value state that
      // we were in when we were switched into this state. Rather than
      // keeping track of this explictly, we observe that the previous
      // state can be determined by additional_allowed_character_.
      if (additional_allowed_character_ == '"')
        HTML_SWITCH_TO(kAttributeValueDoubleQuotedState);
      else if (additional_allowed_character_ == '\'')
        HTML_SWITCH_TO(kAttributeValueSingleQuotedState);
      else if (additional_allowed_character_ == '>')
        HTML_SWITCH_TO(kAttributeValueUnquotedState);
      else
        NOTREACHED_IN_MIGRATION();
    }
    END_STATE()

    HTML_BEGIN_STATE(kAfterAttributeValueQuotedState) {
      if (IsTokenizerWhitespace(cc))
        HTML_ADVANCE_TO(kBeforeAttributeNameState);
      else if (cc == '/')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kSelfClosingStartTagState);
      else if (cc == '>')
        return EmitAndResumeInDataState(source);
      else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else {
        ParseError();
        HTML_RECONSUME_IN(kBeforeAttributeNameState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kSelfClosingStartTagState) {
      if (cc == '>') {
        token_.SetSelfClosing();
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else {
        ParseError();
        HTML_RECONSUME_IN(kBeforeAttributeNameState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kBogusCommentState) {
      token_.BeginComment();
      HTML_RECONSUME_IN(kContinueBogusCommentState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kContinueBogusCommentState) {
      if (cc == '>')
        return EmitAndResumeInDataState(source);
      else if (cc == kEndOfFileMarker)
        return EmitAndReconsumeInDataState();
      else {
        token_.AppendToComment(cc);
        HTML_CONSUME(kContinueBogusCommentState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kMarkupDeclarationOpenState) {
      if (cc == '-') {
        SegmentedString::LookAheadResult result =
            source.LookAhead(html_tokenizer_names::kDashDash);
        if (result == SegmentedString::kDidMatch) {
          source.AdvanceAndASSERT('-');
          source.AdvanceAndASSERT('-');
          token_.BeginComment();
          HTML_SWITCH_TO(kCommentStartState);
        } else if (result == SegmentedString::kNotEnoughCharacters)
          return HaveBufferedCharacterToken();
      } else if (cc == 'D' || cc == 'd') {
        SegmentedString::LookAheadResult result =
            source.LookAheadIgnoringCase(html_tokenizer_names::kDoctype);
        if (result == SegmentedString::kDidMatch) {
          AdvanceStringAndASSERTIgnoringCase(source, "doctype");
          HTML_SWITCH_TO(kDOCTYPEState);
        } else if (result == SegmentedString::kNotEnoughCharacters)
          return HaveBufferedCharacterToken();
      } else if (cc == '[' && ShouldAllowCDATA()) {
        SegmentedString::LookAheadResult result =
            source.LookAhead(html_tokenizer_names::kCdata);
        if (result == SegmentedString::kDidMatch) {
          AdvanceStringAndASSERT(source, "[CDATA[");
          HTML_SWITCH_TO(kCDATASectionState);
        } else if (result == SegmentedString::kNotEnoughCharacters)
          return HaveBufferedCharacterToken();
      }
      ParseError();
      HTML_RECONSUME_IN(kBogusCommentState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kCommentStartState) {
      if (cc == '-')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kCommentStartDashState);
      else if (cc == '>') {
        ParseError();
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        return EmitAndReconsumeInDataState();
      } else {
        token_.AppendToComment(cc);
        HTML_ADVANCE_TO(kCommentState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kCommentStartDashState) {
      if (cc == '-')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kCommentEndState);
      else if (cc == '>') {
        ParseError();
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        return EmitAndReconsumeInDataState();
      } else {
        token_.AppendToComment('-');
        token_.AppendToComment(cc);
        HTML_ADVANCE_TO(kCommentState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kCommentState) {
      if (cc == '-')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kCommentEndDashState);
      else if (cc == kEndOfFileMarker) {
        ParseError();
        return EmitAndReconsumeInDataState();
      } else {
        token_.AppendToComment(cc);
        HTML_CONSUME(kCommentState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kCommentEndDashState) {
      if (cc == '-')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kCommentEndState);
      else if (cc == kEndOfFileMarker) {
        ParseError();
        return EmitAndReconsumeInDataState();
      } else {
        token_.AppendToComment('-');
        token_.AppendToComment(cc);
        HTML_ADVANCE_TO(kCommentState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kCommentEndState) {
      if (cc == '>')
        return EmitAndResumeInDataState(source);
      else if (cc == '!') {
        ParseError();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kCommentEndBangState);
      } else if (cc == '-') {
        ParseError();
        token_.AppendToComment('-');
        HTML_CONSUME_NON_NEWLINE(kCommentEndState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        return EmitAndReconsumeInDataState();
      } else {
        ParseError();
        token_.AppendToComment('-');
        token_.AppendToComment('-');
        token_.AppendToComment(cc);
        HTML_ADVANCE_TO(kCommentState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kCommentEndBangState) {
      if (cc == '-') {
        token_.AppendToComment('-');
        token_.AppendToComment('-');
        token_.AppendToComment('!');
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kCommentEndDashState);
      } else if (cc == '>') {
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        return EmitAndReconsumeInDataState();
      } else {
        token_.AppendToComment('-');
        token_.AppendToComment('-');
        token_.AppendToComment('!');
        token_.AppendToComment(cc);
        HTML_ADVANCE_TO(kCommentState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kDOCTYPEState) {
      if (IsTokenizerWhitespace(cc))
        HTML_ADVANCE_TO(kBeforeDOCTYPENameState);
      else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.BeginDOCTYPE();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        ParseError();
        HTML_RECONSUME_IN(kBeforeDOCTYPENameState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kBeforeDOCTYPENameState) {
      if (!SkipWhitespaces(source, cc))
        return HaveBufferedCharacterToken();
      if (cc == '>') {
        ParseError();
        token_.BeginDOCTYPE();
        token_.SetForceQuirks();
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.BeginDOCTYPE();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        token_.BeginDOCTYPE(ToLowerCaseIfAlpha(cc));
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kDOCTYPENameState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kDOCTYPENameState) {
      if (IsTokenizerWhitespace(cc)) {
        HTML_ADVANCE_TO(kAfterDOCTYPENameState);
      } else if (cc == '>') {
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        token_.AppendToName(ToLowerCaseIfAlpha(cc));
        HTML_CONSUME_NON_NEWLINE(kDOCTYPENameState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAfterDOCTYPENameState) {
      if (!SkipWhitespaces(source, cc))
        return HaveBufferedCharacterToken();
      if (cc == '>')
        return EmitAndResumeInDataState(source);
      else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        if (cc == 'P' || cc == 'p') {
          SegmentedString::LookAheadResult result =
              source.LookAheadIgnoringCase(html_tokenizer_names::kPublic);
          if (result == SegmentedString::kDidMatch) {
            AdvanceStringAndASSERTIgnoringCase(source, "public");
            HTML_SWITCH_TO(kAfterDOCTYPEPublicKeywordState);
          } else if (result == SegmentedString::kNotEnoughCharacters)
            return HaveBufferedCharacterToken();
        } else if (cc == 'S' || cc == 's') {
          SegmentedString::LookAheadResult result =
              source.LookAheadIgnoringCase(html_tokenizer_names::kSystem);
          if (result == SegmentedString::kDidMatch) {
            AdvanceStringAndASSERTIgnoringCase(source, "system");
            HTML_SWITCH_TO(kAfterDOCTYPESystemKeywordState);
          } else if (result == SegmentedString::kNotEnoughCharacters)
            return HaveBufferedCharacterToken();
        }
        ParseError();
        token_.SetForceQuirks();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAfterDOCTYPEPublicKeywordState) {
      if (IsTokenizerWhitespace(cc))
        HTML_ADVANCE_TO(kBeforeDOCTYPEPublicIdentifierState);
      else if (cc == '"') {
        ParseError();
        token_.SetPublicIdentifierToEmptyString();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kDOCTYPEPublicIdentifierDoubleQuotedState);
      } else if (cc == '\'') {
        ParseError();
        token_.SetPublicIdentifierToEmptyString();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kDOCTYPEPublicIdentifierSingleQuotedState);
      } else if (cc == '>') {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        ParseError();
        token_.SetForceQuirks();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kBeforeDOCTYPEPublicIdentifierState) {
      if (!SkipWhitespaces(source, cc))
        return HaveBufferedCharacterToken();
      if (cc == '"') {
        token_.SetPublicIdentifierToEmptyString();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kDOCTYPEPublicIdentifierDoubleQuotedState);
      } else if (cc == '\'') {
        token_.SetPublicIdentifierToEmptyString();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kDOCTYPEPublicIdentifierSingleQuotedState);
      } else if (cc == '>') {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        ParseError();
        token_.SetForceQuirks();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kDOCTYPEPublicIdentifierDoubleQuotedState) {
      if (cc == '"')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kAfterDOCTYPEPublicIdentifierState);
      else if (cc == '>') {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        token_.AppendToPublicIdentifier(cc);
        HTML_CONSUME(kDOCTYPEPublicIdentifierDoubleQuotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kDOCTYPEPublicIdentifierSingleQuotedState) {
      if (cc == '\'')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kAfterDOCTYPEPublicIdentifierState);
      else if (cc == '>') {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        token_.AppendToPublicIdentifier(cc);
        HTML_CONSUME(kDOCTYPEPublicIdentifierSingleQuotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAfterDOCTYPEPublicIdentifierState) {
      if (IsTokenizerWhitespace(cc))
        HTML_ADVANCE_TO(kBetweenDOCTYPEPublicAndSystemIdentifiersState);
      else if (cc == '>')
        return EmitAndResumeInDataState(source);
      else if (cc == '"') {
        ParseError();
        token_.SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kDOCTYPESystemIdentifierDoubleQuotedState);
      } else if (cc == '\'') {
        ParseError();
        token_.SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kDOCTYPESystemIdentifierSingleQuotedState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        ParseError();
        token_.SetForceQuirks();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kBetweenDOCTYPEPublicAndSystemIdentifiersState) {
      if (!SkipWhitespaces(source, cc))
        return HaveBufferedCharacterToken();
      if (cc == '>')
        return EmitAndResumeInDataState(source);
      else if (cc == '"') {
        token_.SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kDOCTYPESystemIdentifierDoubleQuotedState);
      } else if (cc == '\'') {
        token_.SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kDOCTYPESystemIdentifierSingleQuotedState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        ParseError();
        token_.SetForceQuirks();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAfterDOCTYPESystemKeywordState) {
      if (IsTokenizerWhitespace(cc))
        HTML_ADVANCE_TO(kBeforeDOCTYPESystemIdentifierState);
      else if (cc == '"') {
        ParseError();
        token_.SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kDOCTYPESystemIdentifierDoubleQuotedState);
      } else if (cc == '\'') {
        ParseError();
        token_.SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kDOCTYPESystemIdentifierSingleQuotedState);
      } else if (cc == '>') {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        ParseError();
        token_.SetForceQuirks();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kBeforeDOCTYPESystemIdentifierState) {
      if (!SkipWhitespaces(source, cc))
        return HaveBufferedCharacterToken();
      if (cc == '"') {
        token_.SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kDOCTYPESystemIdentifierDoubleQuotedState);
      } else if (cc == '\'') {
        token_.SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(
            kDOCTYPESystemIdentifierSingleQuotedState);
      } else if (cc == '>') {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        ParseError();
        token_.SetForceQuirks();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kDOCTYPESystemIdentifierDoubleQuotedState) {
      if (cc == '"')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kAfterDOCTYPESystemIdentifierState);
      else if (cc == '>') {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        token_.AppendToSystemIdentifier(cc);
        HTML_CONSUME(kDOCTYPESystemIdentifierDoubleQuotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kDOCTYPESystemIdentifierSingleQuotedState) {
      if (cc == '\'')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kAfterDOCTYPESystemIdentifierState);
      else if (cc == '>') {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndResumeInDataState(source);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        token_.AppendToSystemIdentifier(cc);
        HTML_CONSUME(kDOCTYPESystemIdentifierSingleQuotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAfterDOCTYPESystemIdentifierState) {
      if (!SkipWhitespaces(source, cc))
        return HaveBufferedCharacterToken();
      if (cc == '>')
        return EmitAndResumeInDataState(source);
      else if (cc == kEndOfFileMarker) {
        ParseError();
        token_.SetForceQuirks();
        return EmitAndReconsumeInDataState();
      } else {
        ParseError();
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kBogusDOCTYPEState) {
      if (cc == '>')
        return EmitAndResumeInDataState(source);
      else if (cc == kEndOfFileMarker)
        return EmitAndReconsumeInDataState();
      HTML_CONSUME(kBogusDOCTYPEState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kCDATASectionState) {
      if (cc == ']')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kCDATASectionBracketState);
      else if (cc == kEndOfFileMarker)
        HTML_RECONSUME_IN(kDataState);
      else {
        BufferCharacter(cc);
        HTML_CONSUME(kCDATASectionState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kCDATASectionBracketState) {
      if (cc == ']')
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kCDATASectionEndState);
      else {
        BufferCharacter(']');
        HTML_RECONSUME_IN(kCDATASectionState);
      }
    }

    HTML_BEGIN_STATE(kCDATASectionEndState) {
      if (cc == ']') {
        BufferCharacter(']');
        HTML_CONSUME_NON_NEWLINE(kCDATASectionEndState);
      } else if (cc == '>') {
        HTML_ADVANCE_PAST_NON_NEWLINE_TO(kDataState);
      } else {
        BufferCharacter(']');
        BufferCharacter(']');
        HTML_RECONSUME_IN(kCDATASectionState);
      }
    }
    END_STATE()
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool HTMLTokenizer::SkipWhitespaces(SegmentedString& source, UChar& cc) {
  // The character `cc` is usually not a whitespace, so we check it here
  // first, before calling the helper.
  if (!CheckScanFlag(cc, ScanFlags::kWhitespace))
    return true;
  return SkipWhitespacesHelper(source, cc);
}

bool HTMLTokenizer::SkipWhitespacesHelper(SegmentedString& source, UChar& cc) {
  DCHECK(!source.IsEmpty());
  DCHECK(IsTokenizerWhitespace(cc));
  cc = source.CurrentChar();
  while (true) {
    while (CheckScanFlag(cc, ScanFlags::kWhitespaceNotNewline)) {
      cc = source.AdvancePastNonNewline();
    }
    switch (cc) {
      case '\n':
        cc = source.AdvancePastNewlineAndUpdateLineNumber();
        break;
      case '\r':
        if (!input_stream_preprocessor_.AdvancePastCarriageReturn(source, cc))
          return false;
        break;
      case '\0':
        if (!input_stream_preprocessor_.ProcessNullCharacter(source, cc))
          return false;
        if (cc == kEndOfFileMarker)
          return true;
        break;
      default:
        return true;
    }
  }
}

bool HTMLTokenizer::EmitData(SegmentedString& source, UChar cc) {
  token_.EnsureIsCharacterToken();
  if (cc == '\n')  // We could be pointing to '\r'.
    cc = source.CurrentChar();
  while (true) {
    while (!CheckScanFlag(cc, ScanFlags::kCharacterTokenSpecial)) {
      token_.AppendToCharacter(cc);
      cc = source.AdvancePastNonNewline();
    }
    switch (cc) {
      case '&':
        state_ = kCharacterReferenceInDataState;
        source.AdvanceAndASSERT('&');
        if (!ProcessEntity(source))
          return true;
        state_ = kDataState;
        if (source.IsEmpty())
          return true;
        cc = source.CurrentChar();
        break;
      case '\n':
        token_.AppendToCharacter(cc);
        cc = source.AdvancePastNewlineAndUpdateLineNumber();
        break;
      case '\r':
        token_.AppendToCharacter('\n');  // Canonize newline.
        if (!input_stream_preprocessor_.AdvancePastCarriageReturn(source, cc))
          return true;
        break;
      case '<':
        return true;
      case '\0':
        if (!input_stream_preprocessor_.ProcessNullCharacter(source, cc))
          return true;
        if (cc == kEndOfFileMarker)
          return EmitEndOfFile(source);
        break;
      case '{':
        DCHECK_EQ(strlen(kChildNodePartStartMarker),
                  strlen(kChildNodePartEndMarker));
        static_assert(kChildNodePartStartMarker[0] == '{');
        static_assert(kChildNodePartEndMarker[0] == '{');
        if (ShouldAllowDOMParts()) {
          auto result = source.LookAhead(kChildNodePartStartMarker);
          if (result == SegmentedString::kDidMatch) {
            state_ = kChildNodePartStartState;
            if (token_.Characters().IsEmpty()) {
              // TODO(crbug.com/1453291) If we have `<div parseparts>{{#}}`,
              // then we will be in a character token that is empty, which is
              // not good. Add a space for now to get around this, but it'd
              // be better to not get to EmitData at all from kDataState at all
              // in this case and just go directly to kChildNodePartStartState.
              token_.AppendToCharacter(' ');
            }
            // Emit the character data up to this point, then switch to
            // kChildNodePartStartState.
            return true;
          } else if (result == SegmentedString::kNotEnoughCharacters) {
            // TODO(crbug.com/1453291) If we never receive the rest of the start
            // marker, we'll get in an infinite loop here. This might be the
            // same problem that happens for <!DOCTYPE>, in crbug.com/1141343
            // and crbug.com/985307.
            return false;
          }
          result = source.LookAhead(kChildNodePartEndMarker);
          if (result == SegmentedString::kDidMatch) {
            state_ = kChildNodePartEndState;
            if (token_.Characters().IsEmpty()) {
              // TODO(crbug.com/1453291) If we have `{{#}}{{/}}`, then we will
              // be in a character token that is empty (between the markers),
              // which is not good. Add a space for now to get around this, but
              // it'd be better to not get to EmitData at all from kDataState at
              // all in this case and just go directly to
              // kChildNodePartEndState.
              token_.AppendToCharacter(' ');
            }
            // Emit the character data up to this point, then switch to
            // kChildNodePartEndState.
            return true;
          } else if (result == SegmentedString::kNotEnoughCharacters) {
            return false;
          }
        }
        token_.AppendToCharacter(cc);
        cc = source.AdvancePastNonNewline();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
}

bool HTMLTokenizer::EmitPLAINTEXT(SegmentedString& source, UChar cc) {
  token_.EnsureIsCharacterToken();
  if (cc == '\n')  // We could be pointing to '\r'.
    cc = source.CurrentChar();
  while (true) {
    while (!CheckScanFlag(cc, ScanFlags::kNullOrNewline)) {
      token_.AppendToCharacter(cc);
      cc = source.AdvancePastNonNewline();
    }
    switch (cc) {
      case '\n':
        token_.AppendToCharacter(cc);
        cc = source.AdvancePastNewlineAndUpdateLineNumber();
        break;
      case '\r':
        token_.AppendToCharacter('\n');  // Canonize newline.
        if (!input_stream_preprocessor_.AdvancePastCarriageReturn(source, cc))
          return true;
        break;
      case '\0':
        if (!input_stream_preprocessor_.ProcessNullCharacter(source, cc))
          return true;
        if (cc == kEndOfFileMarker)
          return EmitEndOfFile(source);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
}

String HTMLTokenizer::BufferedCharacters() const {
  // FIXME: Add a DCHECK about state_.
  StringBuilder characters;
  characters.ReserveCapacity(NumberOfBufferedCharacters());
  characters.Append('<');
  characters.Append('/');
  characters.Append(temporary_buffer_.data(), temporary_buffer_.size());
  return characters.ToString();
}

void HTMLTokenizer::UpdateStateFor(const HTMLToken& token) {
  if (!token.GetName().IsEmpty()) {
    UpdateStateFor(
        lookupHTMLTag(token.GetName().data(), token.GetName().size()));
  }
}

void HTMLTokenizer::UpdateStateFor(html_names::HTMLTag tag) {
  auto state = SpeculativeStateForTag(tag);
  if (state)
    SetState(*state);
}

std::optional<HTMLTokenizer::State> HTMLTokenizer::SpeculativeStateForTag(
    html_names::HTMLTag tag) const {
  switch (tag) {
    case html_names::HTMLTag::kTextarea:
    case html_names::HTMLTag::kTitle:
      return HTMLTokenizer::kRCDATAState;
    case html_names::HTMLTag::kPlaintext:
      return HTMLTokenizer::kPLAINTEXTState;
    case html_names::HTMLTag::kScript:
      return HTMLTokenizer::kScriptDataState;
    case html_names::HTMLTag::kStyle:
    case html_names::HTMLTag::kIFrame:
    case html_names::HTMLTag::kXmp:
    case html_names::HTMLTag::kNoembed:
    case html_names::HTMLTag::kNoframes:
      return HTMLTokenizer::kRAWTEXTState;
    case html_names::HTMLTag::kNoscript:
      if (options_.scripting_flag)
        return HTMLTokenizer::kRAWTEXTState;
      return std::nullopt;
    default:
      return std::nullopt;
  }
}

inline bool HTMLTokenizer::TemporaryBufferIs(const String& expected_string) {
  return VectorEqualsString(temporary_buffer_, expected_string);
}

inline void HTMLTokenizer::AddToPossibleEndTag(LChar cc) {
  DCHECK(IsEndTagBufferingState(state_));
  buffered_end_tag_name_.AddChar(cc);
}

inline bool HTMLTokenizer::IsAppropriateEndTag() {
  if (buffered_end_tag_name_.size() != appropriate_end_tag_name_.size())
    return false;

  return Equal(buffered_end_tag_name_.data(), appropriate_end_tag_name_.data(),
               buffered_end_tag_name_.size());
}

inline void HTMLTokenizer::ParseError() {
#if DCHECK_IS_ON()
  DVLOG(1) << "Not implemented.";
#endif
}

}  // namespace blink
