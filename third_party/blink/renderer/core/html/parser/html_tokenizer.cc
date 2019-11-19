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

static inline UChar ToLowerCase(UChar cc) {
  DCHECK(IsASCIIAlpha(cc));
  return cc | 0x20;
}

static inline UChar ToLowerCaseIfAlpha(UChar cc) {
  return cc | (IsASCIIUpper(cc) ? 0x20 : 0);
}

static inline bool VectorEqualsString(const Vector<LChar, 32>& vector,
                                      const String& string) {
  if (vector.size() != string.length())
    return false;

  if (!string.length())
    return true;

  return Equal(string.Impl(), vector.data(), vector.size());
}

#define HTML_BEGIN_STATE(stateName) BEGIN_STATE(HTMLTokenizer, stateName)
#define HTML_RECONSUME_IN(stateName) RECONSUME_IN(HTMLTokenizer, stateName)
#define HTML_ADVANCE_TO(stateName) ADVANCE_TO(HTMLTokenizer, stateName)
#define HTML_CONSUME(stateName) CONSUME(HTMLTokenizer, stateName)
#define HTML_SWITCH_TO(stateName) SWITCH_TO(HTMLTokenizer, stateName)

HTMLTokenizer::HTMLTokenizer(const HTMLParserOptions& options)
    : input_stream_preprocessor_(this), options_(options) {
  Reset();
}

HTMLTokenizer::~HTMLTokenizer() = default;

void HTMLTokenizer::Reset() {
  state_ = HTMLTokenizer::kDataState;
  token_ = nullptr;
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

bool HTMLTokenizer::FlushBufferedEndTag(SegmentedString& source) {
  DCHECK(token_->GetType() == HTMLToken::kCharacter ||
         token_->GetType() == HTMLToken::kUninitialized);
  source.AdvanceAndUpdateLineNumber();
  if (token_->GetType() == HTMLToken::kCharacter)
    return true;
  token_->BeginEndTag(buffered_end_tag_name_);
  buffered_end_tag_name_.clear();
  appropriate_end_tag_name_.clear();
  temporary_buffer_.clear();
  return false;
}

#define FLUSH_AND_ADVANCE_TO(stateName)                               \
  do {                                                                \
    state_ = HTMLTokenizer::stateName;                                \
    if (FlushBufferedEndTag(source))                                  \
      return true;                                                    \
    if (source.IsEmpty() || !input_stream_preprocessor_.Peek(source)) \
      return HaveBufferedCharacterToken();                            \
    cc = input_stream_preprocessor_.NextInputCharacter();             \
    goto stateName;                                                   \
  } while (false)

bool HTMLTokenizer::FlushEmitAndResumeIn(SegmentedString& source,
                                         HTMLTokenizer::State state) {
  state_ = state;
  FlushBufferedEndTag(source);
  return true;
}

bool HTMLTokenizer::NextToken(SegmentedString& source, HTMLToken& token) {
  // If we have a token in progress, then we're supposed to be called back
  // with the same token so we can finish it.
  DCHECK(!token_ || token_ == &token ||
         token.GetType() == HTMLToken::kUninitialized);
  token_ = &token;

  if (!buffered_end_tag_name_.IsEmpty() && !IsEndTagBufferingState(state_)) {
    // FIXME: This should call flushBufferedEndTag().
    // We started an end tag during our last iteration.
    token_->BeginEndTag(buffered_end_tag_name_);
    buffered_end_tag_name_.clear();
    appropriate_end_tag_name_.clear();
    temporary_buffer_.clear();
    if (state_ == HTMLTokenizer::kDataState) {
      // We're back in the data state, so we must be done with the tag.
      return true;
    }
  }

  if (source.IsEmpty() || !input_stream_preprocessor_.Peek(source))
    return HaveBufferedCharacterToken();
  UChar cc = input_stream_preprocessor_.NextInputCharacter();

  // Source: http://www.whatwg.org/specs/web-apps/current-work/#tokenisation0
  switch (state_) {
    HTML_BEGIN_STATE(kDataState) {
      if (cc == '&')
        HTML_ADVANCE_TO(kCharacterReferenceInDataState);
      else if (cc == '<') {
        if (token_->GetType() == HTMLToken::kCharacter) {
          // We have a bunch of character tokens queued up that we
          // are emitting lazily here.
          return true;
        }
        HTML_ADVANCE_TO(kTagOpenState);
      } else if (cc == kEndOfFileMarker)
        return EmitEndOfFile(source);
      else {
        BufferCharacter(cc);
        HTML_CONSUME(kDataState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kCharacterReferenceInDataState) {
      if (!ProcessEntity(source))
        return HaveBufferedCharacterToken();
      HTML_SWITCH_TO(kDataState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kRCDATAState) {
      if (cc == '&')
        HTML_ADVANCE_TO(kCharacterReferenceInRCDATAState);
      else if (cc == '<')
        HTML_ADVANCE_TO(kRCDATALessThanSignState);
      else if (cc == kEndOfFileMarker)
        return EmitEndOfFile(source);
      else {
        BufferCharacter(cc);
        HTML_CONSUME(kRCDATAState);
      }
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
        HTML_ADVANCE_TO(kRAWTEXTLessThanSignState);
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
        HTML_ADVANCE_TO(kScriptDataLessThanSignState);
      else if (cc == kEndOfFileMarker)
        return EmitEndOfFile(source);
      else {
        BufferCharacter(cc);
        HTML_CONSUME(kScriptDataState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kPLAINTEXTState) {
      if (cc == kEndOfFileMarker)
        return EmitEndOfFile(source);
      BufferCharacter(cc);
      HTML_CONSUME(kPLAINTEXTState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kTagOpenState) {
      if (cc == '!') {
        HTML_ADVANCE_TO(kMarkupDeclarationOpenState);
      } else if (cc == '/') {
        HTML_ADVANCE_TO(kEndTagOpenState);
      } else if (IsASCIIAlpha(cc)) {
        token_->BeginStartTag(ToLowerCase(cc));
        HTML_ADVANCE_TO(kTagNameState);
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
        token_->BeginEndTag(static_cast<LChar>(ToLowerCase(cc)));
        appropriate_end_tag_name_.clear();
        HTML_ADVANCE_TO(kTagNameState);
      } else if (cc == '>') {
        ParseError();
        HTML_ADVANCE_TO(kDataState);
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
      if (IsTokenizerWhitespace(cc)) {
        HTML_ADVANCE_TO(kBeforeAttributeNameState);
      } else if (cc == '/') {
        HTML_ADVANCE_TO(kSelfClosingStartTagState);
      } else if (cc == '>') {
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else {
        token_->AppendToName(ToLowerCaseIfAlpha(cc));
        HTML_CONSUME(kTagNameState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kRCDATALessThanSignState) {
      if (cc == '/') {
        temporary_buffer_.clear();
        DCHECK(buffered_end_tag_name_.IsEmpty());
        HTML_ADVANCE_TO(kRCDATAEndTagOpenState);
      } else {
        BufferCharacter('<');
        HTML_RECONSUME_IN(kRCDATAState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kRCDATAEndTagOpenState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.push_back(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_ADVANCE_TO(kRCDATAEndTagNameState);
      } else {
        BufferCharacter('<');
        BufferCharacter('/');
        HTML_RECONSUME_IN(kRCDATAState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kRCDATAEndTagNameState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.push_back(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_CONSUME(kRCDATAEndTagNameState);
      } else {
        if (IsTokenizerWhitespace(cc)) {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.push_back(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO(kBeforeAttributeNameState);
          }
        } else if (cc == '/') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.push_back(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO(kSelfClosingStartTagState);
          }
        } else if (cc == '>') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.push_back(static_cast<LChar>(cc));
            return FlushEmitAndResumeIn(source, HTMLTokenizer::kDataState);
          }
        }
        BufferCharacter('<');
        BufferCharacter('/');
        token_->AppendToCharacter(temporary_buffer_);
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
        HTML_ADVANCE_TO(kRAWTEXTEndTagOpenState);
      } else {
        BufferCharacter('<');
        HTML_RECONSUME_IN(kRAWTEXTState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kRAWTEXTEndTagOpenState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.push_back(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_ADVANCE_TO(kRAWTEXTEndTagNameState);
      } else {
        BufferCharacter('<');
        BufferCharacter('/');
        HTML_RECONSUME_IN(kRAWTEXTState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kRAWTEXTEndTagNameState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.push_back(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_CONSUME(kRAWTEXTEndTagNameState);
      } else {
        if (IsTokenizerWhitespace(cc)) {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.push_back(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO(kBeforeAttributeNameState);
          }
        } else if (cc == '/') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.push_back(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO(kSelfClosingStartTagState);
          }
        } else if (cc == '>') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.push_back(static_cast<LChar>(cc));
            return FlushEmitAndResumeIn(source, HTMLTokenizer::kDataState);
          }
        }
        BufferCharacter('<');
        BufferCharacter('/');
        token_->AppendToCharacter(temporary_buffer_);
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
        HTML_ADVANCE_TO(kScriptDataEndTagOpenState);
      } else if (cc == '!') {
        BufferCharacter('<');
        BufferCharacter('!');
        HTML_ADVANCE_TO(kScriptDataEscapeStartState);
      } else {
        BufferCharacter('<');
        HTML_RECONSUME_IN(kScriptDataState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEndTagOpenState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.push_back(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_ADVANCE_TO(kScriptDataEndTagNameState);
      } else {
        BufferCharacter('<');
        BufferCharacter('/');
        HTML_RECONSUME_IN(kScriptDataState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEndTagNameState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.push_back(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_CONSUME(kScriptDataEndTagNameState);
      } else {
        if (IsTokenizerWhitespace(cc)) {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.push_back(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO(kBeforeAttributeNameState);
          }
        } else if (cc == '/') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.push_back(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO(kSelfClosingStartTagState);
          }
        } else if (cc == '>') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.push_back(static_cast<LChar>(cc));
            return FlushEmitAndResumeIn(source, HTMLTokenizer::kDataState);
          }
        }
        BufferCharacter('<');
        BufferCharacter('/');
        token_->AppendToCharacter(temporary_buffer_);
        buffered_end_tag_name_.clear();
        temporary_buffer_.clear();
        HTML_RECONSUME_IN(kScriptDataState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEscapeStartState) {
      if (cc == '-') {
        BufferCharacter(cc);
        HTML_ADVANCE_TO(kScriptDataEscapeStartDashState);
      } else
        HTML_RECONSUME_IN(kScriptDataState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEscapeStartDashState) {
      if (cc == '-') {
        BufferCharacter(cc);
        HTML_ADVANCE_TO(kScriptDataEscapedDashDashState);
      } else
        HTML_RECONSUME_IN(kScriptDataState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEscapedState) {
      if (cc == '-') {
        BufferCharacter(cc);
        HTML_ADVANCE_TO(kScriptDataEscapedDashState);
      } else if (cc == '<')
        HTML_ADVANCE_TO(kScriptDataEscapedLessThanSignState);
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
        HTML_ADVANCE_TO(kScriptDataEscapedDashDashState);
      } else if (cc == '<')
        HTML_ADVANCE_TO(kScriptDataEscapedLessThanSignState);
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
        HTML_CONSUME(kScriptDataEscapedDashDashState);
      } else if (cc == '<')
        HTML_ADVANCE_TO(kScriptDataEscapedLessThanSignState);
      else if (cc == '>') {
        BufferCharacter(cc);
        HTML_ADVANCE_TO(kScriptDataState);
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
        HTML_ADVANCE_TO(kScriptDataEscapedEndTagOpenState);
      } else if (IsASCIIAlpha(cc)) {
        BufferCharacter('<');
        BufferCharacter(cc);
        temporary_buffer_.clear();
        temporary_buffer_.push_back(static_cast<LChar>(ToLowerCase(cc)));
        HTML_ADVANCE_TO(kScriptDataDoubleEscapeStartState);
      } else {
        BufferCharacter('<');
        HTML_RECONSUME_IN(kScriptDataEscapedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEscapedEndTagOpenState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.push_back(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_ADVANCE_TO(kScriptDataEscapedEndTagNameState);
      } else {
        BufferCharacter('<');
        BufferCharacter('/');
        HTML_RECONSUME_IN(kScriptDataEscapedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataEscapedEndTagNameState) {
      if (IsASCIIAlpha(cc)) {
        temporary_buffer_.push_back(static_cast<LChar>(cc));
        AddToPossibleEndTag(static_cast<LChar>(ToLowerCase(cc)));
        HTML_CONSUME(kScriptDataEscapedEndTagNameState);
      } else {
        if (IsTokenizerWhitespace(cc)) {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.push_back(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO(kBeforeAttributeNameState);
          }
        } else if (cc == '/') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.push_back(static_cast<LChar>(cc));
            FLUSH_AND_ADVANCE_TO(kSelfClosingStartTagState);
          }
        } else if (cc == '>') {
          if (IsAppropriateEndTag()) {
            temporary_buffer_.push_back(static_cast<LChar>(cc));
            return FlushEmitAndResumeIn(source, HTMLTokenizer::kDataState);
          }
        }
        BufferCharacter('<');
        BufferCharacter('/');
        token_->AppendToCharacter(temporary_buffer_);
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
        temporary_buffer_.push_back(static_cast<LChar>(ToLowerCase(cc)));
        HTML_CONSUME(kScriptDataDoubleEscapeStartState);
      } else
        HTML_RECONSUME_IN(kScriptDataEscapedState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kScriptDataDoubleEscapedState) {
      if (cc == '-') {
        BufferCharacter(cc);
        HTML_ADVANCE_TO(kScriptDataDoubleEscapedDashState);
      } else if (cc == '<') {
        BufferCharacter(cc);
        HTML_ADVANCE_TO(kScriptDataDoubleEscapedLessThanSignState);
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
        HTML_ADVANCE_TO(kScriptDataDoubleEscapedDashDashState);
      } else if (cc == '<') {
        BufferCharacter(cc);
        HTML_ADVANCE_TO(kScriptDataDoubleEscapedLessThanSignState);
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
        HTML_CONSUME(kScriptDataDoubleEscapedDashDashState);
      } else if (cc == '<') {
        BufferCharacter(cc);
        HTML_ADVANCE_TO(kScriptDataDoubleEscapedLessThanSignState);
      } else if (cc == '>') {
        BufferCharacter(cc);
        HTML_ADVANCE_TO(kScriptDataState);
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
        HTML_ADVANCE_TO(kScriptDataDoubleEscapeEndState);
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
        temporary_buffer_.push_back(static_cast<LChar>(ToLowerCase(cc)));
        HTML_CONSUME(kScriptDataDoubleEscapeEndState);
      } else
        HTML_RECONSUME_IN(kScriptDataDoubleEscapedState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kBeforeAttributeNameState) {
      if (IsTokenizerWhitespace(cc)) {
        HTML_CONSUME(kBeforeAttributeNameState);
      } else if (cc == '/') {
        HTML_ADVANCE_TO(kSelfClosingStartTagState);
      } else if (cc == '>') {
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else {
        if (cc == '"' || cc == '\'' || cc == '<' || cc == '=')
          ParseError();
        token_->AddNewAttribute();
        token_->BeginAttributeName(source.NumberOfCharactersConsumed());
        token_->AppendToAttributeName(ToLowerCaseIfAlpha(cc));
        HTML_ADVANCE_TO(kAttributeNameState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAttributeNameState) {
      if (IsTokenizerWhitespace(cc)) {
        token_->EndAttributeName(source.NumberOfCharactersConsumed());
        HTML_ADVANCE_TO(kAfterAttributeNameState);
      } else if (cc == '/') {
        token_->EndAttributeName(source.NumberOfCharactersConsumed());
        HTML_ADVANCE_TO(kSelfClosingStartTagState);
      } else if (cc == '=') {
        token_->EndAttributeName(source.NumberOfCharactersConsumed());
        HTML_ADVANCE_TO(kBeforeAttributeValueState);
      } else if (cc == '>') {
        token_->EndAttributeName(source.NumberOfCharactersConsumed());
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->EndAttributeName(source.NumberOfCharactersConsumed());
        HTML_RECONSUME_IN(kDataState);
      } else {
        if (cc == '"' || cc == '\'' || cc == '<' || cc == '=')
          ParseError();
        token_->AppendToAttributeName(ToLowerCaseIfAlpha(cc));
        HTML_CONSUME(kAttributeNameState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAfterAttributeNameState) {
      if (IsTokenizerWhitespace(cc)) {
        HTML_CONSUME(kAfterAttributeNameState);
      } else if (cc == '/') {
        HTML_ADVANCE_TO(kSelfClosingStartTagState);
      } else if (cc == '=') {
        HTML_ADVANCE_TO(kBeforeAttributeValueState);
      } else if (cc == '>') {
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else {
        if (cc == '"' || cc == '\'' || cc == '<')
          ParseError();
        token_->AddNewAttribute();
        token_->BeginAttributeName(source.NumberOfCharactersConsumed());
        token_->AppendToAttributeName(ToLowerCaseIfAlpha(cc));
        HTML_ADVANCE_TO(kAttributeNameState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kBeforeAttributeValueState) {
      if (IsTokenizerWhitespace(cc))
        HTML_CONSUME(kBeforeAttributeValueState);
      else if (cc == '"') {
        token_->BeginAttributeValue(source.NumberOfCharactersConsumed() + 1);
        HTML_ADVANCE_TO(kAttributeValueDoubleQuotedState);
      } else if (cc == '&') {
        token_->BeginAttributeValue(source.NumberOfCharactersConsumed());
        HTML_RECONSUME_IN(kAttributeValueUnquotedState);
      } else if (cc == '\'') {
        token_->BeginAttributeValue(source.NumberOfCharactersConsumed() + 1);
        HTML_ADVANCE_TO(kAttributeValueSingleQuotedState);
      } else if (cc == '>') {
        ParseError();
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        HTML_RECONSUME_IN(kDataState);
      } else {
        if (cc == '<' || cc == '=' || cc == '`')
          ParseError();
        token_->BeginAttributeValue(source.NumberOfCharactersConsumed());
        token_->AppendToAttributeValue(cc);
        HTML_ADVANCE_TO(kAttributeValueUnquotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAttributeValueDoubleQuotedState) {
      if (cc == '"') {
        token_->EndAttributeValue(source.NumberOfCharactersConsumed());
        HTML_ADVANCE_TO(kAfterAttributeValueQuotedState);
      } else if (cc == '&') {
        additional_allowed_character_ = '"';
        HTML_ADVANCE_TO(kCharacterReferenceInAttributeValueState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->EndAttributeValue(source.NumberOfCharactersConsumed());
        HTML_RECONSUME_IN(kDataState);
      } else {
        token_->AppendToAttributeValue(cc);
        HTML_CONSUME(kAttributeValueDoubleQuotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAttributeValueSingleQuotedState) {
      if (cc == '\'') {
        token_->EndAttributeValue(source.NumberOfCharactersConsumed());
        HTML_ADVANCE_TO(kAfterAttributeValueQuotedState);
      } else if (cc == '&') {
        additional_allowed_character_ = '\'';
        HTML_ADVANCE_TO(kCharacterReferenceInAttributeValueState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->EndAttributeValue(source.NumberOfCharactersConsumed());
        HTML_RECONSUME_IN(kDataState);
      } else {
        token_->AppendToAttributeValue(cc);
        HTML_CONSUME(kAttributeValueSingleQuotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAttributeValueUnquotedState) {
      if (IsTokenizerWhitespace(cc)) {
        token_->EndAttributeValue(source.NumberOfCharactersConsumed());
        HTML_ADVANCE_TO(kBeforeAttributeNameState);
      } else if (cc == '&') {
        additional_allowed_character_ = '>';
        HTML_ADVANCE_TO(kCharacterReferenceInAttributeValueState);
      } else if (cc == '>') {
        token_->EndAttributeValue(source.NumberOfCharactersConsumed());
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->EndAttributeValue(source.NumberOfCharactersConsumed());
        HTML_RECONSUME_IN(kDataState);
      } else {
        if (cc == '"' || cc == '\'' || cc == '<' || cc == '=' || cc == '`')
          ParseError();
        token_->AppendToAttributeValue(cc);
        HTML_CONSUME(kAttributeValueUnquotedState);
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
        token_->AppendToAttributeValue('&');
      } else {
        for (unsigned i = 0; i < decoded_entity.length; ++i)
          token_->AppendToAttributeValue(decoded_entity.data[i]);
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
        NOTREACHED();
    }
    END_STATE()

    HTML_BEGIN_STATE(kAfterAttributeValueQuotedState) {
      if (IsTokenizerWhitespace(cc))
        HTML_ADVANCE_TO(kBeforeAttributeNameState);
      else if (cc == '/')
        HTML_ADVANCE_TO(kSelfClosingStartTagState);
      else if (cc == '>')
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
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
        token_->SetSelfClosing();
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
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
      token_->BeginComment();
      HTML_RECONSUME_IN(kContinueBogusCommentState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kContinueBogusCommentState) {
      if (cc == '>')
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      else if (cc == kEndOfFileMarker)
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      else {
        token_->AppendToComment(cc);
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
          token_->BeginComment();
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
        HTML_ADVANCE_TO(kCommentStartDashState);
      else if (cc == '>') {
        ParseError();
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        token_->AppendToComment(cc);
        HTML_ADVANCE_TO(kCommentState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kCommentStartDashState) {
      if (cc == '-')
        HTML_ADVANCE_TO(kCommentEndState);
      else if (cc == '>') {
        ParseError();
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        token_->AppendToComment('-');
        token_->AppendToComment(cc);
        HTML_ADVANCE_TO(kCommentState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kCommentState) {
      if (cc == '-')
        HTML_ADVANCE_TO(kCommentEndDashState);
      else if (cc == kEndOfFileMarker) {
        ParseError();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        token_->AppendToComment(cc);
        HTML_CONSUME(kCommentState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kCommentEndDashState) {
      if (cc == '-')
        HTML_ADVANCE_TO(kCommentEndState);
      else if (cc == kEndOfFileMarker) {
        ParseError();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        token_->AppendToComment('-');
        token_->AppendToComment(cc);
        HTML_ADVANCE_TO(kCommentState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kCommentEndState) {
      if (cc == '>')
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      else if (cc == '!') {
        ParseError();
        HTML_ADVANCE_TO(kCommentEndBangState);
      } else if (cc == '-') {
        ParseError();
        token_->AppendToComment('-');
        HTML_CONSUME(kCommentEndState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        ParseError();
        token_->AppendToComment('-');
        token_->AppendToComment('-');
        token_->AppendToComment(cc);
        HTML_ADVANCE_TO(kCommentState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kCommentEndBangState) {
      if (cc == '-') {
        token_->AppendToComment('-');
        token_->AppendToComment('-');
        token_->AppendToComment('!');
        HTML_ADVANCE_TO(kCommentEndDashState);
      } else if (cc == '>')
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      else if (cc == kEndOfFileMarker) {
        ParseError();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        token_->AppendToComment('-');
        token_->AppendToComment('-');
        token_->AppendToComment('!');
        token_->AppendToComment(cc);
        HTML_ADVANCE_TO(kCommentState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kDOCTYPEState) {
      if (IsTokenizerWhitespace(cc))
        HTML_ADVANCE_TO(kBeforeDOCTYPENameState);
      else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->BeginDOCTYPE();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        ParseError();
        HTML_RECONSUME_IN(kBeforeDOCTYPENameState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kBeforeDOCTYPENameState) {
      if (IsTokenizerWhitespace(cc)) {
        HTML_CONSUME(kBeforeDOCTYPENameState);
      } else if (cc == '>') {
        ParseError();
        token_->BeginDOCTYPE();
        token_->SetForceQuirks();
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->BeginDOCTYPE();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        token_->BeginDOCTYPE(ToLowerCaseIfAlpha(cc));
        HTML_ADVANCE_TO(kDOCTYPENameState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kDOCTYPENameState) {
      if (IsTokenizerWhitespace(cc)) {
        HTML_ADVANCE_TO(kAfterDOCTYPENameState);
      } else if (cc == '>') {
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        token_->AppendToName(ToLowerCaseIfAlpha(cc));
        HTML_CONSUME(kDOCTYPENameState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAfterDOCTYPENameState) {
      if (IsTokenizerWhitespace(cc))
        HTML_CONSUME(kAfterDOCTYPENameState);
      if (cc == '>')
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
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
        token_->SetForceQuirks();
        HTML_ADVANCE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAfterDOCTYPEPublicKeywordState) {
      if (IsTokenizerWhitespace(cc))
        HTML_ADVANCE_TO(kBeforeDOCTYPEPublicIdentifierState);
      else if (cc == '"') {
        ParseError();
        token_->SetPublicIdentifierToEmptyString();
        HTML_ADVANCE_TO(kDOCTYPEPublicIdentifierDoubleQuotedState);
      } else if (cc == '\'') {
        ParseError();
        token_->SetPublicIdentifierToEmptyString();
        HTML_ADVANCE_TO(kDOCTYPEPublicIdentifierSingleQuotedState);
      } else if (cc == '>') {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        ParseError();
        token_->SetForceQuirks();
        HTML_ADVANCE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kBeforeDOCTYPEPublicIdentifierState) {
      if (IsTokenizerWhitespace(cc))
        HTML_CONSUME(kBeforeDOCTYPEPublicIdentifierState);
      else if (cc == '"') {
        token_->SetPublicIdentifierToEmptyString();
        HTML_ADVANCE_TO(kDOCTYPEPublicIdentifierDoubleQuotedState);
      } else if (cc == '\'') {
        token_->SetPublicIdentifierToEmptyString();
        HTML_ADVANCE_TO(kDOCTYPEPublicIdentifierSingleQuotedState);
      } else if (cc == '>') {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        ParseError();
        token_->SetForceQuirks();
        HTML_ADVANCE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kDOCTYPEPublicIdentifierDoubleQuotedState) {
      if (cc == '"')
        HTML_ADVANCE_TO(kAfterDOCTYPEPublicIdentifierState);
      else if (cc == '>') {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        token_->AppendToPublicIdentifier(cc);
        HTML_CONSUME(kDOCTYPEPublicIdentifierDoubleQuotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kDOCTYPEPublicIdentifierSingleQuotedState) {
      if (cc == '\'')
        HTML_ADVANCE_TO(kAfterDOCTYPEPublicIdentifierState);
      else if (cc == '>') {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        token_->AppendToPublicIdentifier(cc);
        HTML_CONSUME(kDOCTYPEPublicIdentifierSingleQuotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAfterDOCTYPEPublicIdentifierState) {
      if (IsTokenizerWhitespace(cc))
        HTML_ADVANCE_TO(kBetweenDOCTYPEPublicAndSystemIdentifiersState);
      else if (cc == '>')
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      else if (cc == '"') {
        ParseError();
        token_->SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_TO(kDOCTYPESystemIdentifierDoubleQuotedState);
      } else if (cc == '\'') {
        ParseError();
        token_->SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_TO(kDOCTYPESystemIdentifierSingleQuotedState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        ParseError();
        token_->SetForceQuirks();
        HTML_ADVANCE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kBetweenDOCTYPEPublicAndSystemIdentifiersState) {
      if (IsTokenizerWhitespace(cc))
        HTML_CONSUME(kBetweenDOCTYPEPublicAndSystemIdentifiersState);
      else if (cc == '>')
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      else if (cc == '"') {
        token_->SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_TO(kDOCTYPESystemIdentifierDoubleQuotedState);
      } else if (cc == '\'') {
        token_->SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_TO(kDOCTYPESystemIdentifierSingleQuotedState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        ParseError();
        token_->SetForceQuirks();
        HTML_ADVANCE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAfterDOCTYPESystemKeywordState) {
      if (IsTokenizerWhitespace(cc))
        HTML_ADVANCE_TO(kBeforeDOCTYPESystemIdentifierState);
      else if (cc == '"') {
        ParseError();
        token_->SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_TO(kDOCTYPESystemIdentifierDoubleQuotedState);
      } else if (cc == '\'') {
        ParseError();
        token_->SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_TO(kDOCTYPESystemIdentifierSingleQuotedState);
      } else if (cc == '>') {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        ParseError();
        token_->SetForceQuirks();
        HTML_ADVANCE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kBeforeDOCTYPESystemIdentifierState) {
      if (IsTokenizerWhitespace(cc))
        HTML_CONSUME(kBeforeDOCTYPESystemIdentifierState);
      if (cc == '"') {
        token_->SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_TO(kDOCTYPESystemIdentifierDoubleQuotedState);
      } else if (cc == '\'') {
        token_->SetSystemIdentifierToEmptyString();
        HTML_ADVANCE_TO(kDOCTYPESystemIdentifierSingleQuotedState);
      } else if (cc == '>') {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        ParseError();
        token_->SetForceQuirks();
        HTML_ADVANCE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kDOCTYPESystemIdentifierDoubleQuotedState) {
      if (cc == '"')
        HTML_ADVANCE_TO(kAfterDOCTYPESystemIdentifierState);
      else if (cc == '>') {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        token_->AppendToSystemIdentifier(cc);
        HTML_CONSUME(kDOCTYPESystemIdentifierDoubleQuotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kDOCTYPESystemIdentifierSingleQuotedState) {
      if (cc == '\'')
        HTML_ADVANCE_TO(kAfterDOCTYPESystemIdentifierState);
      else if (cc == '>') {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      } else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        token_->AppendToSystemIdentifier(cc);
        HTML_CONSUME(kDOCTYPESystemIdentifierSingleQuotedState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kAfterDOCTYPESystemIdentifierState) {
      if (IsTokenizerWhitespace(cc))
        HTML_CONSUME(kAfterDOCTYPESystemIdentifierState);
      else if (cc == '>')
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      else if (cc == kEndOfFileMarker) {
        ParseError();
        token_->SetForceQuirks();
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      } else {
        ParseError();
        HTML_ADVANCE_TO(kBogusDOCTYPEState);
      }
    }
    END_STATE()

    HTML_BEGIN_STATE(kBogusDOCTYPEState) {
      if (cc == '>')
        return EmitAndResumeIn(source, HTMLTokenizer::kDataState);
      else if (cc == kEndOfFileMarker)
        return EmitAndReconsumeIn(source, HTMLTokenizer::kDataState);
      HTML_CONSUME(kBogusDOCTYPEState);
    }
    END_STATE()

    HTML_BEGIN_STATE(kCDATASectionState) {
      if (cc == ']')
        HTML_ADVANCE_TO(kCDATASectionBracketState);
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
        HTML_ADVANCE_TO(kCDATASectionEndState);
      else {
        BufferCharacter(']');
        HTML_RECONSUME_IN(kCDATASectionState);
      }
    }

    HTML_BEGIN_STATE(kCDATASectionEndState) {
      if (cc == ']') {
        BufferCharacter(']');
        HTML_CONSUME(kCDATASectionEndState);
      } else if (cc == '>') {
        HTML_ADVANCE_TO(kDataState);
      } else {
        BufferCharacter(']');
        BufferCharacter(']');
        HTML_RECONSUME_IN(kCDATASectionState);
      }
    }
    END_STATE()
  }

  NOTREACHED();
  return false;
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

void HTMLTokenizer::UpdateStateFor(const String& tag_name) {
  if (ThreadSafeMatch(tag_name, html_names::kTextareaTag) ||
      ThreadSafeMatch(tag_name, html_names::kTitleTag))
    SetState(HTMLTokenizer::kRCDATAState);
  else if (ThreadSafeMatch(tag_name, html_names::kPlaintextTag))
    SetState(HTMLTokenizer::kPLAINTEXTState);
  else if (ThreadSafeMatch(tag_name, html_names::kScriptTag))
    SetState(HTMLTokenizer::kScriptDataState);
  else if (ThreadSafeMatch(tag_name, html_names::kStyleTag) ||
           ThreadSafeMatch(tag_name, html_names::kIFrameTag) ||
           ThreadSafeMatch(tag_name, html_names::kXmpTag) ||
           ThreadSafeMatch(tag_name, html_names::kNoembedTag) ||
           ThreadSafeMatch(tag_name, html_names::kNoframesTag) ||
           (ThreadSafeMatch(tag_name, html_names::kNoscriptTag) &&
            options_.script_enabled))
    SetState(HTMLTokenizer::kRAWTEXTState);
}

inline bool HTMLTokenizer::TemporaryBufferIs(const String& expected_string) {
  return VectorEqualsString(temporary_buffer_, expected_string);
}

inline void HTMLTokenizer::AddToPossibleEndTag(LChar cc) {
  DCHECK(IsEndTagBufferingState(state_));
  buffered_end_tag_name_.push_back(cc);
}

inline bool HTMLTokenizer::IsAppropriateEndTag() {
  if (buffered_end_tag_name_.size() != appropriate_end_tag_name_.size())
    return false;

  wtf_size_t num_characters = buffered_end_tag_name_.size();

  for (wtf_size_t i = 0; i < num_characters; i++) {
    if (buffered_end_tag_name_[i] != appropriate_end_tag_name_[i])
      return false;
  }

  return true;
}

inline void HTMLTokenizer::ParseError() {
#if DCHECK_IS_ON()
  DVLOG(1) << "Not implemented.";
#endif
}

}  // namespace blink
