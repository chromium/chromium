// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  return VisitCharacters(value, [](auto chars) {
    while (!chars.empty()) {
      // Consume any whitespaces.
      while (!chars.empty() && IsASCIISpace(chars.front())) {
        chars = chars.template subspan<1u>();
      }
      // Consume a directive name.
      bool directive_name_found = false;
      while (!chars.empty() && IsCSPDirectiveNameCharacter(chars.front())) {
        chars = chars.template subspan<1u>();
        directive_name_found = true;
      }
      // Consume the directive value (if any), but only if there is a directive
      // name followed by at least one whitespace.
      if (directive_name_found) {
        bool space_found = false;
        while (!chars.empty() && IsASCIISpace(chars.front())) {
          chars = chars.template subspan<1u>();
          space_found = true;
        }
        if (space_found) {
          while (!chars.empty() &&
                 IsCSPDirectiveValueCharacter(chars.front())) {
            chars = chars.template subspan<1u>();
          }
        }
      }
      if (chars.empty()) {
        return true;
      }
      // There should be at least one ';'.
      bool semicolon_found = false;
      while (!chars.empty() && chars.front() == ';') {
        chars = chars.template subspan<1u>();
        semicolon_found = true;
      }
      if (!semicolon_found)
        return false;
    }
    return true;
  });
}

}  // namespace blink
