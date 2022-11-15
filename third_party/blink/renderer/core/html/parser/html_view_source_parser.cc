/*
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

#include "third_party/blink/renderer/core/html/parser/html_view_source_parser.h"

#include <memory>
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_options.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"

namespace blink {

namespace {

HTMLParserOptions CreateParserOptions(HTMLViewSourceDocument& document) {
  HTMLParserOptions options(&document);
  options.track_attributes_ranges = true;
  return options;
}

}  // namespace

HTMLViewSourceParser::HTMLViewSourceParser(HTMLViewSourceDocument& document,
                                           const String& mime_type)
    : DecodedDataDocumentParser(document),
      tokenizer_(
          std::make_unique<HTMLTokenizer>(CreateParserOptions(document))) {
  if (mime_type != "text/html" && !MIMETypeRegistry::IsXMLMIMEType(mime_type))
    tokenizer_->SetState(HTMLTokenizer::kPLAINTEXTState);
}

void HTMLViewSourceParser::PumpTokenizer() {
  while (true) {
    StartTracker(input_.Current(), tokenizer_.get(), token_);
    HTMLToken* token = tokenizer_->NextToken(input_.Current());
    if (!token)
      return;
    token_ = token;
    EndTracker(input_.Current(), tokenizer_.get());

    GetDocument()->AddSource(SourceForToken(*token_), *token_,
                             tokenizer_->attributes_ranges(), token_start_);

    if (token_->GetType() == HTMLToken::kStartTag)
      tokenizer_->UpdateStateFor(*token_);
    token_->Clear();
    tokenizer_->attributes_ranges().Clear();
  }
}

void HTMLViewSourceParser::Append(const String& input) {
  input_.AppendToEnd(input);
  PumpTokenizer();
}

void HTMLViewSourceParser::Finish() {
  Flush();
  if (!input_.HaveSeenEndOfFile())
    input_.MarkEndOfFile();

  if (!IsDetached()) {
    PumpTokenizer();
    GetDocument()->FinishedParsing();
  }
}

void HTMLViewSourceParser::StartTracker(SegmentedString& current_input,
                                        HTMLTokenizer* tokenizer,
                                        HTMLToken* token) {
  if (!tracker_is_started_ && (!token || token->IsUninitialized())) {
    previous_source_.Clear();
    if (NeedToCheckTokenizerBuffer(tokenizer) &&
        tokenizer->NumberOfBufferedCharacters())
      previous_source_ = tokenizer->BufferedCharacters();
  } else {
    previous_source_.Append(current_source_);
  }

  tracker_is_started_ = true;
  current_source_ = current_input;

  token_start_ =
      current_source_.NumberOfCharactersConsumed() - previous_source_.length();
}

void HTMLViewSourceParser::EndTracker(SegmentedString& current_input,
                                      HTMLTokenizer* tokenizer) {
  tracker_is_started_ = false;

  cached_source_for_token_ = String();

  // FIXME: This work should really be done by the HTMLTokenizer.
  wtf_size_t number_of_buffered_characters = 0u;
  if (NeedToCheckTokenizerBuffer(tokenizer)) {
    number_of_buffered_characters = tokenizer->NumberOfBufferedCharacters();
  }
  token_end_ = current_input.NumberOfCharactersConsumed() -
               number_of_buffered_characters - token_start_;
}

String HTMLViewSourceParser::SourceForToken(const HTMLToken& token) {
  if (!cached_source_for_token_.empty())
    return cached_source_for_token_;

  wtf_size_t length;
  if (token.GetType() == HTMLToken::kEndOfFile) {
    // Consume the remainder of the input, omitting the null character we use to
    // mark the end of the file.
    length = previous_source_.length() + current_source_.length() - 1;
  } else {
    length = token_end_;
  }

  StringBuilder source;
  source.ReserveCapacity(length);

  size_t i = 0;
  for (; i < length && !previous_source_.IsEmpty(); ++i) {
    source.Append(previous_source_.CurrentChar());
    previous_source_.Advance();
  }
  for (; i < length; ++i) {
    DCHECK(!current_source_.IsEmpty());
    source.Append(current_source_.CurrentChar());
    current_source_.Advance();
  }

  cached_source_for_token_ = source.ToString();
  return cached_source_for_token_;
}

bool HTMLViewSourceParser::NeedToCheckTokenizerBuffer(
    HTMLTokenizer* tokenizer) {
  HTMLTokenizer::State state = tokenizer->GetState();
  // The temporary buffer must not be used unconditionally, because in some
  // states (e.g. ScriptDataDoubleEscapedStartState), data is appended to
  // both the temporary buffer and the token itself.
  return state == HTMLTokenizer::kDataState ||
         HTMLTokenizer::IsEndTagBufferingState(state);
}

}  // namespace blink
