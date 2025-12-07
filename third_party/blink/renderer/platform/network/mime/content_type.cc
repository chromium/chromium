/*
 * Copyright (C) 2006, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2009 Google Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/network/mime/content_type.h"

namespace blink {

ContentType::ContentType(const String& content_type)
    : type_(content_type.StripWhiteSpace()) {}

static bool IsASCIIQuote(UChar c) {
  return c == '"';
}

String ContentType::Parameter(StringView parameter_name) const {
  Vector<String> parameters;
  ParseParameters(parameters);

  for (auto& parameter : parameters) {
    String stripped_parameter = parameter.StripWhiteSpace();
    wtf_size_t separator_pos = stripped_parameter.find('=');
    if (separator_pos != kNotFound) {
      String attribute =
          stripped_parameter.Left(separator_pos).StripWhiteSpace();
      if (EqualIgnoringASCIICase(attribute, parameter_name)) {
        return stripped_parameter.Substring(separator_pos + 1)
            .StripWhiteSpace()
            .RemoveCharacters(IsASCIIQuote);
      }
    }
  }

  return String();
}

String ContentType::GetType() const {
  // "type" can have parameters after a semi-colon, strip them
  const wtf_size_t semicolon_index = type_.find(';');
  if (semicolon_index != kNotFound) {
    return type_.Left(semicolon_index).StripWhiteSpace();
  }
  return type_;
}

void ContentType::ParseParameters(Vector<String>& result) const {
  unsigned cur_pos = 0;
  unsigned end_pos = type_.length();
  unsigned start_pos = 0;
  bool is_quote = false;

  while (cur_pos < end_pos) {
    const UChar ch = type_[cur_pos];
    if (!is_quote && ch == ';') {
      if (cur_pos != start_pos) {
        result.push_back(type_.Substring(start_pos, cur_pos - start_pos));
      }
      start_pos = cur_pos + 1;
    } else if (ch == '"') {
      is_quote = !is_quote;
    }
    cur_pos++;
  }

  if (start_pos != end_pos)
    result.push_back(type_.Substring(start_pos));
}

}  // namespace blink
