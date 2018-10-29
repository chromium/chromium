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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_COMPACT_HTML_TOKEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_COMPACT_HTML_TOKEN_H_

#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class QualifiedName;

class CORE_EXPORT CompactHTMLToken {
  DISALLOW_NEW();

 public:
  struct Attribute {
    DISALLOW_NEW();

   public:
    Attribute(const String& name, const String& value)
        : name_(name), value_(value) {}

    const String& GetName() const { return name_; }
    const String& Value() const { return value_; }

    // We don't create a new 8-bit String because it doesn't save memory.
    const String& Value8BitIfNecessary() const { return value_; }

   private:
    String name_;
    String value_;
  };

  CompactHTMLToken(const HTMLToken*, const TextPosition&);

  HTMLToken::TokenType GetType() const {
    return static_cast<HTMLToken::TokenType>(type_);
  }
  const String& Data() const { return data_; }
  bool SelfClosing() const { return self_closing_; }
  bool IsAll8BitData() const { return is_all8_bit_data_; }
  const Vector<Attribute>& Attributes() const { return attributes_; }
  const Attribute* GetAttributeItem(const QualifiedName&) const;
  const TextPosition& GetTextPosition() const { return text_position_; }

  // There is only 1 DOCTYPE token per document, so to avoid increasing the
  // size of CompactHTMLToken, we just use the attributes_ vector.
  const String& PublicIdentifier() const { return attributes_[0].GetName(); }
  const String& SystemIdentifier() const { return attributes_[0].Value(); }
  bool DoctypeForcesQuirks() const { return doctype_forces_quirks_; }

 private:
  unsigned type_ : 4;
  unsigned self_closing_ : 1;
  unsigned is_all8_bit_data_ : 1;
  unsigned doctype_forces_quirks_ : 1;

  String data_;  // "name", "characters", or "data" depending on type_
  Vector<Attribute> attributes_;
  TextPosition text_position_;
};

typedef Vector<CompactHTMLToken> CompactHTMLTokenStream;

}  // namespace blink

#endif
