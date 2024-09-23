// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"

#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

namespace {

bool IsCSPDirectiveNameCharacter(UChar c) {
  return IsASCIIAlphanumeric(c) || c == '-';
}

bool IsCSPDirectiveValueCharacter(UChar c) {
  return IsASCIISpace(c) || (IsASCIIPrintable(c) && c != ',' && c != ';');
}

}  // namespace

bool MatchesTheSerializedCSPGrammar(const String& value) {
  return WTF::VisitCharacters(value, [](auto chars) {
    const auto* it = chars.data();
    const auto* end = chars.data() + chars.size();

    while (it < end) {
      // Consume any whitespaces.
      while (it < end && IsASCIISpace(*it))
        it++;

      // Consume a directive name.
      bool directive_name_found = false;
      while (it < end && IsCSPDirectiveNameCharacter(*it)) {
        it++;
        directive_name_found = true;
      }

      // Consume the directive value (if any), but only if there is a directive
      // name followed by at least one whitespace.
      if (directive_name_found) {
        bool space_found = false;
        while (it < end && IsASCIISpace(*it)) {
          it++;
          space_found = true;
        }
        if (space_found) {
          while (it < end && IsCSPDirectiveValueCharacter(*it))
            it++;
        }
      }

      if (it == end)
        return true;

      // There should be at least one ';'.
      bool semicolon_found = false;
      while (it < end && *it == ';') {
        it++;
        semicolon_found = true;
      }
      if (!semicolon_found)
        return false;
    }
    return true;
  });
}

}  // namespace blink
