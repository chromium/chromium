/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/parser/compact_html_token.h"

#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"

namespace blink {

struct SameSizeAsCompactHTMLToken {
  unsigned bitfields;
  String data;
  Vector<Attribute> vector;
  TextPosition text_position;
};

static_assert(sizeof(CompactHTMLToken) == sizeof(SameSizeAsCompactHTMLToken),
              "CompactHTMLToken should stay small");

CompactHTMLToken::CompactHTMLToken(const HTMLToken* token,
                                   const TextPosition& text_position)
    : type_(token->GetType()),
      is_all8_bit_data_(false),
      doctype_forces_quirks_(false),
      text_position_(text_position) {
  switch (type_) {
    case HTMLToken::kUninitialized:
      NOTREACHED();
      break;
    case HTMLToken::DOCTYPE: {
      data_ = AttemptStaticStringCreation(token->GetName(), kLikely8Bit);

      // There is only 1 DOCTYPE token per document, so to avoid increasing the
      // size of CompactHTMLToken, we just use the attributes_ vector.
      attributes_.push_back(Attribute(
          AttemptStaticStringCreation(token->PublicIdentifier(), kLikely8Bit),
          String(token->SystemIdentifier())));
      doctype_forces_quirks_ = token->ForceQuirks();
      break;
    }
    case HTMLToken::kEndOfFile:
      break;
    case HTMLToken::kStartTag:
      attributes_.ReserveInitialCapacity(token->Attributes().size());
      for (const HTMLToken::Attribute& attribute : token->Attributes())
        attributes_.push_back(
            Attribute(attribute.NameAttemptStaticStringCreation(),
                      attribute.Value8BitIfNecessary()));
      FALLTHROUGH;
    case HTMLToken::kEndTag:
      self_closing_ = token->SelfClosing();
      FALLTHROUGH;
    case HTMLToken::kComment:
    case HTMLToken::kCharacter: {
      is_all8_bit_data_ = token->IsAll8BitData();
      data_ = AttemptStaticStringCreation(
          token->Data(), token->IsAll8BitData() ? kForce8Bit : kForce16Bit);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

const CompactHTMLToken::Attribute* CompactHTMLToken::GetAttributeItem(
    const QualifiedName& name) const {
  for (unsigned i = 0; i < attributes_.size(); ++i) {
    if (ThreadSafeMatch(attributes_.at(i).GetName(), name))
      return &attributes_.at(i);
  }
  return nullptr;
}

}  // namespace blink
