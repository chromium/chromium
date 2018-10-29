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

ContentType::ContentType(const String& content_type) : type_(content_type) {}

String ContentType::Parameter(const String& parameter_name) const {
  String parameter_value;
  String stripped_type = type_.StripWhiteSpace();

  // a MIME type can have one or more "param=value" after a semi-colon, and
  // separated from each other by semi-colons
  wtf_size_t semi = stripped_type.find(';');
  if (semi != kNotFound) {
    wtf_size_t start =
        stripped_type.FindIgnoringASCIICase(parameter_name, semi + 1);
    if (start != kNotFound) {
      start = stripped_type.find('=', start + parameter_name.length());
      if (start != kNotFound) {
        wtf_size_t quote = stripped_type.find('\"', start + 1);
        wtf_size_t end = stripped_type.find('\"', start + 2);
        if (quote != kNotFound && end != kNotFound) {
          start = quote;
        } else {
          end = stripped_type.find(';', start + 1);
          if (end == kNotFound)
            end = stripped_type.length();
        }
        parameter_value = stripped_type.Substring(start + 1, end - (start + 1))
                              .StripWhiteSpace();
      }
    }
  }

  return parameter_value;
}

String ContentType::GetType() const {
  String stripped_type = type_.StripWhiteSpace();

  // "type" can have parameters after a semi-colon, strip them
  wtf_size_t semi = stripped_type.find(';');
  if (semi != kNotFound)
    stripped_type = stripped_type.Left(semi).StripWhiteSpace();

  return stripped_type;
}

}  // namespace blink
