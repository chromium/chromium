/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_TOKENIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_TOKENIZER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_options.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/core/html/parser/input_stream_preprocessor.h"
#include "third_party/blink/renderer/platform/text/segmented_string.h"

namespace blink {

class CORE_EXPORT HTMLTokenizer {
  USING_FAST_MALLOC(HTMLTokenizer);

 public:
  explicit HTMLTokenizer(const HTMLParserOptions&);
  ~HTMLTokenizer();

  void Reset();

  enum State {
    kDataState,
    kCharacterReferenceInDataState,
    kRCDATAState,
    kCharacterReferenceInRCDATAState,
    kRAWTEXTState,
    kScriptDataState,
    kPLAINTEXTState,
    kTagOpenState,
    kEndTagOpenState,
    kTagNameState,
    kRCDATALessThanSignState,
    kRCDATAEndTagOpenState,
    kRCDATAEndTagNameState,
    kRAWTEXTLessThanSignState,
    kRAWTEXTEndTagOpenState,
    kRAWTEXTEndTagNameState,
    kScriptDataLessThanSignState,
    kScriptDataEndTagOpenState,
    kScriptDataEndTagNameState,
    kScriptDataEscapeStartState,
    kScriptDataEscapeStartDashState,
    kScriptDataEscapedState,
    kScriptDataEscapedDashState,
    kScriptDataEscapedDashDashState,
    kScriptDataEscapedLessThanSignState,
    kScriptDataEscapedEndTagOpenState,
    kScriptDataEscapedEndTagNameState,
    kScriptDataDoubleEscapeStartState,
    kScriptDataDoubleEscapedState,
    kScriptDataDoubleEscapedDashState,
    kScriptDataDoubleEscapedDashDashState,
    kScriptDataDoubleEscapedLessThanSignState,
    kScriptDataDoubleEscapeEndState,
    kBeforeAttributeNameState,
    kAttributeNameState,
    kAfterAttributeNameState,
    kBeforeAttributeValueState,
    kAttributeValueDoubleQuotedState,
    kAttributeValueSingleQuotedState,
    kAttributeValueUnquotedState,
    kCharacterReferenceInAttributeValueState,
    kAfterAttributeValueQuotedState,
    kSelfClosingStartTagState,
    kBogusCommentState,
    // The ContinueBogusCommentState is not in the HTML5 spec, but we use
    // it internally to keep track of whether we've started the bogus
    // comment token yet.
    kContinueBogusCommentState,
    kMarkupDeclarationOpenState,
    kCommentStartState,
    kCommentStartDashState,
    kCommentState,
    kCommentEndDashState,
    kCommentEndState,
    kCommentEndBangState,
    kDOCTYPEState,
    kBeforeDOCTYPENameState,
    kDOCTYPENameState,
    kAfterDOCTYPENameState,
    kAfterDOCTYPEPublicKeywordState,
    kBeforeDOCTYPEPublicIdentifierState,
    kDOCTYPEPublicIdentifierDoubleQuotedState,
    kDOCTYPEPublicIdentifierSingleQuotedState,
    kAfterDOCTYPEPublicIdentifierState,
    kBetweenDOCTYPEPublicAndSystemIdentifiersState,
    kAfterDOCTYPESystemKeywordState,
    kBeforeDOCTYPESystemIdentifierState,
    kDOCTYPESystemIdentifierDoubleQuotedState,
    kDOCTYPESystemIdentifierSingleQuotedState,
    kAfterDOCTYPESystemIdentifierState,
    kBogusDOCTYPEState,
    kCDATASectionState,
    kCDATASectionBracketState,
    kCDATASectionEndState,
  };

  // This function returns true if it emits a token. Otherwise, callers
  // must provide the same (in progress) token on the next call (unless
  // they call reset() first).
  bool NextToken(SegmentedString&, HTMLToken&);

  // Returns a copy of any characters buffered internally by the tokenizer.
  // The tokenizer buffers characters when searching for the </script> token
  // that terminates a script element.
  String BufferedCharacters() const;

  wtf_size_t NumberOfBufferedCharacters() const {
    // Notice that we add 2 to the length of the temporary_buffer_ to
    // account for the "</" characters, which are effectively buffered in
    // the tokenizer's state machine.
    return temporary_buffer_.size() ? temporary_buffer_.size() + 2 : 0;
  }

  // Updates the tokenizer's state according to the given tag name. This is
  // an approximation of how the tree builder would update the tokenizer's
  // state. This method is useful for approximating HTML tokenization. To
  // get exactly the correct tokenization, you need the real tree builder.
  //
  // The main failures in the approximation are as follows:
  //
  //  * The first set of character tokens emitted for a <pre> element might
  //    contain an extra leading newline.
  //  * The replacement of U+0000 with U+FFFD will not be sensitive to the
  //    tree builder's insertion mode.
  //  * CDATA sections in foreign content will be tokenized as bogus comments
  //    instead of as character tokens.
  //
  void UpdateStateFor(const String& tag_name);

  bool ForceNullCharacterReplacement() const {
    return force_null_character_replacement_;
  }
  void SetForceNullCharacterReplacement(bool value) {
    force_null_character_replacement_ = value;
  }

  bool ShouldAllowCDATA() const { return should_allow_cdata_; }
  void SetShouldAllowCDATA(bool value) { should_allow_cdata_ = value; }

  State GetState() const { return state_; }
  void SetState(State state) { state_ = state; }

  inline bool ShouldSkipNullCharacters() const {
    return !force_null_character_replacement_ &&
           (state_ == HTMLTokenizer::kDataState ||
            state_ == HTMLTokenizer::kRCDATAState ||
            state_ == HTMLTokenizer::kRAWTEXTState);
  }

  inline static bool IsEndTagBufferingState(HTMLTokenizer::State state) {
    switch (state) {
      case HTMLTokenizer::kRCDATAEndTagOpenState:
      case HTMLTokenizer::kRCDATAEndTagNameState:
      case HTMLTokenizer::kRAWTEXTEndTagOpenState:
      case HTMLTokenizer::kRAWTEXTEndTagNameState:
      case HTMLTokenizer::kScriptDataEndTagOpenState:
      case HTMLTokenizer::kScriptDataEndTagNameState:
      case HTMLTokenizer::kScriptDataEscapedEndTagOpenState:
      case HTMLTokenizer::kScriptDataEscapedEndTagNameState:
        return true;
      default:
        return false;
    }
  }

 private:
  inline bool ProcessEntity(SegmentedString&);

  inline void ParseError();

  inline void BufferCharacter(UChar character) {
    DCHECK_NE(character, kEndOfFileMarker);
    token_->EnsureIsCharacterToken();
    token_->AppendToCharacter(character);
  }

  inline bool EmitAndResumeIn(SegmentedString& source, State state) {
    SaveEndTagNameIfNeeded();
    state_ = state;
    source.AdvanceAndUpdateLineNumber();
    return true;
  }

  inline bool EmitAndReconsumeIn(SegmentedString&, State state) {
    SaveEndTagNameIfNeeded();
    state_ = state;
    return true;
  }

  inline bool EmitEndOfFile(SegmentedString& source) {
    if (HaveBufferedCharacterToken())
      return true;
    state_ = HTMLTokenizer::kDataState;
    source.AdvanceAndUpdateLineNumber();
    token_->Clear();
    token_->MakeEndOfFile();
    return true;
  }

  inline bool FlushEmitAndResumeIn(SegmentedString&, State);

  // Return whether we need to emit a character token before dealing with
  // the buffered end tag.
  inline bool FlushBufferedEndTag(SegmentedString&);
  inline bool TemporaryBufferIs(const String&);

  // Sometimes we speculatively consume input characters and we don't
  // know whether they represent end tags or RCDATA, etc. These
  // functions help manage these state.
  inline void AddToPossibleEndTag(LChar cc);

  inline void SaveEndTagNameIfNeeded() {
    DCHECK_NE(token_->GetType(), HTMLToken::kUninitialized);
    if (token_->GetType() == HTMLToken::kStartTag)
      appropriate_end_tag_name_ = token_->GetName();
  }
  inline bool IsAppropriateEndTag();

  inline bool HaveBufferedCharacterToken() {
    return token_->GetType() == HTMLToken::kCharacter;
  }

  State state_;
  bool force_null_character_replacement_;
  bool should_allow_cdata_;

  // token_ is owned by the caller. If NextToken is not on the stack,
  // this member might be pointing to unallocated memory.
  HTMLToken* token_;

  // http://www.whatwg.org/specs/web-apps/current-work/#additional-allowed-character
  UChar additional_allowed_character_;

  // http://www.whatwg.org/specs/web-apps/current-work/#preprocessing-the-input-stream
  InputStreamPreprocessor<HTMLTokenizer> input_stream_preprocessor_;

  Vector<UChar, 32> appropriate_end_tag_name_;

  // http://www.whatwg.org/specs/web-apps/current-work/#temporary-buffer
  Vector<LChar, 32> temporary_buffer_;

  // We occationally want to emit both a character token and an end tag
  // token (e.g., when lexing script). We buffer the name of the end tag
  // token here so we remember it next time we re-enter the tokenizer.
  Vector<LChar, 32> buffered_end_tag_name_;

  HTMLParserOptions options_;

  DISALLOW_COPY_AND_ASSIGN(HTMLTokenizer);
};

}  // namespace blink

#endif
