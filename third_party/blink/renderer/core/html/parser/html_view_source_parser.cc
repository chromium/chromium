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
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_options.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"

namespace blink {

HTMLViewSourceParser::HTMLViewSourceParser(HTMLViewSourceDocument& document,
                                           const String& mime_type)
    : DecodedDataDocumentParser(document),
      tokenizer_(
          std::make_unique<HTMLTokenizer>(HTMLParserOptions(&document))) {
  if (mime_type != "text/html" && !DOMImplementation::IsXMLMIMEType(mime_type))
    tokenizer_->SetState(HTMLTokenizer::kPLAINTEXTState);
}

void HTMLViewSourceParser::PumpTokenizer() {
  while (true) {
    source_tracker_.Start(input_.Current(), tokenizer_.get(), token_);
    if (!tokenizer_->NextToken(input_.Current(), token_))
      return;
    source_tracker_.end(input_.Current(), tokenizer_.get(), token_);

    GetDocument()->AddSource(source_tracker_.SourceForToken(token_), token_);

    // FIXME: The tokenizer should do this work for us.
    if (token_.GetType() == HTMLToken::kStartTag)
      tokenizer_->UpdateStateFor(
          AttemptStaticStringCreation(token_.GetName(), kLikely8Bit));
    token_.Clear();
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

}  // namespace blink
