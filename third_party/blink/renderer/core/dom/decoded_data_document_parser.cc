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

#include "third_party/blink/renderer/core/dom/decoded_data_document_parser.h"

#include <memory>
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_encoding_data.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

DecodedDataDocumentParser::DecodedDataDocumentParser(Document& document)
    : DocumentParser(&document), needs_decoder_(true) {}

DecodedDataDocumentParser::~DecodedDataDocumentParser() = default;

void DecodedDataDocumentParser::SetDecoder(
    std::unique_ptr<TextResourceDecoder> decoder) {
  // If the decoder is explicitly unset rather than having ownership
  // transferred away by takeDecoder(), we need to make sure it's recreated
  // next time data is appended.
  needs_decoder_ = !decoder;
  decoder_ = std::move(decoder);
}

TextResourceDecoder* DecodedDataDocumentParser::Decoder() {
  return decoder_.get();
}

std::unique_ptr<TextResourceDecoder> DecodedDataDocumentParser::TakeDecoder() {
  return std::move(decoder_);
}

void DecodedDataDocumentParser::AppendBytes(const char* data, size_t length) {
  TRACE_EVENT0("loading", "DecodedDataDocumentParser::AppendBytes");
  if (!length)
    return;

  // This should be checking isStopped(), but XMLDocumentParser prematurely
  // stops parsing when handling an XSLT processing instruction and still
  // needs to receive decoded bytes.
  if (IsDetached())
    return;

  String decoded = decoder_->Decode(data, length);
  UpdateDocument(decoded);
}

void DecodedDataDocumentParser::Flush() {
  // This should be checking isStopped(), but XMLDocumentParser prematurely
  // stops parsing when handling an XSLT processing instruction and still
  // needs to receive decoded bytes.
  if (IsDetached())
    return;

  // null decoder indicates there is no data received.
  // We have nothing to do in that case.
  if (!decoder_)
    return;

  String remaining_data = decoder_->Flush();
  UpdateDocument(remaining_data);
}

void DecodedDataDocumentParser::UpdateDocument(String& decoded_data) {
  GetDocument()->SetEncodingData(DocumentEncodingData(*decoder_.get()));

  if (!decoded_data.IsEmpty())
    Append(decoded_data);
}

}  // namespace blink
