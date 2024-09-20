/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/css_markup.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_family.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

// "ident" from the CSS tokenizer, minus backslash-escape sequences
static bool IsCSSTokenizerIdentifier(const StringView& string) {
  unsigned length = string.length();

  if (!length) {
    return false;
  }

  return WTF::VisitCharacters(string, [](auto chars) {
    const auto* p = chars.data();
    const auto* end = p + chars.size();

    // -?
    if (p != end && p[0] == '-') {
      ++p;
    }

    // {nmstart}
    if (p == end || !IsNameStartCodePoint(p[0])) {
      return false;
    }
    ++p;

    // {nmchar}*
    for (; p != end; ++p) {
      if (!IsNameCodePoint(p[0])) {
        return false;
      }
    }

    return true;
  });
}

static void SerializeCharacter(UChar32 c, StringBuilder& append_to) {
  append_to.Append('\\');
  append_to.Append(c);
}

static void SerializeCharacterAsCodePoint(UChar32 c, StringBuilder& append_to) {
  append_to.AppendFormat("\\%x ", c);
}

void SerializeIdentifier(const String& identifier,
                         StringBuilder& append_to,
                         bool skip_start_checks) {
  bool is_first = !skip_start_checks;
  bool is_second = false;
  bool is_first_char_hyphen = false;
  unsigned index = 0;
  while (index < identifier.length()) {
    UChar32 c = identifier.CharacterStartingAt(index);
    if (c == 0) {
      // Check for lone surrogate which characterStartingAt does not return.
      c = identifier[index];
    }

    index += U16_LENGTH(c);

    if (c == 0) {
      append_to.Append(0xfffd);
    } else if (c <= 0x1f || c == 0x7f ||
               (0x30 <= c && c <= 0x39 &&
                (is_first || (is_second && is_first_char_hyphen)))) {
      SerializeCharacterAsCodePoint(c, append_to);
    } else if (c == 0x2d && is_first && index == identifier.length()) {
      SerializeCharacter(c, append_to);
    } else if (0x80 <= c || c == 0x2d || c == 0x5f ||
               (0x30 <= c && c <= 0x39) || (0x41 <= c && c <= 0x5a) ||
               (0x61 <= c && c <= 0x7a)) {
      append_to.Append(c);
    } else {
      SerializeCharacter(c, append_to);
    }

    if (is_first) {
      is_first = false;
      is_second = true;
      is_first_char_hyphen = (c == 0x2d);
    } else if (is_second) {
      is_second = false;
    }
  }
}

void SerializeString(const String& string, StringBuilder& append_to) {
  append_to.Append('\"');

  unsigned index = 0;
  while (index < string.length()) {
    UChar32 c = string.CharacterStartingAt(index);
    index += U16_LENGTH(c);

    if (c <= 0x1f || c == 0x7f) {
      SerializeCharacterAsCodePoint(c, append_to);
    } else if (c == 0x22 || c == 0x5c) {
      SerializeCharacter(c, append_to);
    } else {
      append_to.Append(c);
    }
  }

  append_to.Append('\"');
}

String SerializeString(const String& string) {
  StringBuilder builder;
  SerializeString(string, builder);
  return builder.ReleaseString();
}

String SerializeURI(const String& string) {
  return "url(" + SerializeString(string) + ")";
}

String SerializeFontFamily(const AtomicString& string) {
  // Some <font-family> values are serialized without quotes.
  // See https://github.com/w3c/csswg-drafts/issues/5846
  return (css_parsing_utils::IsCSSWideKeyword(string) ||
          css_parsing_utils::IsDefaultKeyword(string) ||
          FontFamily::InferredTypeFor(string) ==
              FontFamily::Type::kGenericFamily ||
          !IsCSSTokenizerIdentifier(string))
             ? SerializeString(string)
             : string;
}

}  // namespace blink
