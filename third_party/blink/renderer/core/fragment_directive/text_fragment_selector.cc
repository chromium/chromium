// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector.h"

#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// Escapes special chars that can be part of text fragment directive, including
// hyphen (-), ampersand (&), and comma (,).
String EscapeSelectorSpecialCharacters(const String& target_text) {
  String escaped_str = EncodeWithURLEscapeSequences(target_text);
  escaped_str.Replace("-", "%2D");
  return escaped_str;
}

// Used after parsing out individual terms from the full string microsyntax to
// tell if the resulting string contains only valid characters.
bool IsValidTerm(const StringView& term) {
  // Should only be called on terms after splitting on ',' and '&', which are
  // also invalid chars.
  DCHECK(!term.contains(','));
  DCHECK(!term.contains('&'));

  if (term.empty())
    return false;

  return !term.contains('-');
}

bool IsPrefix(const StringView& term) {
  return term.ends_with('-');
}

bool IsSuffix(const StringView& term) {
  return term.starts_with('-');
}

}  // namespace

TextFragmentSelector TextFragmentSelector::FromTextDirective(
    const StringView& directive) {
  DEFINE_STATIC_LOCAL(const TextFragmentSelector, kInvalidSelector, (kInvalid));
  SelectorType type;
  StringView start;
  StringView end;
  StringView prefix;
  StringView suffix;

  DCHECK(!directive.contains('&'));

  if (HasInvalidUrlEscapeSequences(directive)) {
    return kInvalidSelector;
  }

  Vector<StringView> terms = directive.Split(',');

  if (terms.empty() || terms.size() > 4)
    return kInvalidSelector;

  if (IsPrefix(terms.front())) {
    prefix = terms.front();
    prefix.remove_suffix(1);
    terms.erase(terms.begin());

    if (!IsValidTerm(prefix) || terms.empty())
      return kInvalidSelector;
  }

  if (IsSuffix(terms.back())) {
    suffix = terms.back();
    suffix.remove_prefix(1);
    terms.pop_back();

    if (!IsValidTerm(suffix) || terms.empty())
      return kInvalidSelector;
  }

  DCHECK(!terms.empty());
  if (terms.size() > 2)
    return kInvalidSelector;

  type = kExact;
  start = terms.front();
  if (!IsValidTerm(start))
    return kInvalidSelector;
  terms.erase(terms.begin());

  if (!terms.empty()) {
    type = kRange;
    end = terms.front();
    if (!IsValidTerm(end))
      return kInvalidSelector;

    terms.erase(terms.begin());
  }

  DCHECK(terms.empty());

  constexpr auto kMode = DecodeUrlMode::kUtf8;
  return TextFragmentSelector(type, DecodeUrlEscapeSequences(start, kMode),
                              DecodeUrlEscapeSequences(end, kMode),
                              DecodeUrlEscapeSequences(prefix, kMode),
                              DecodeUrlEscapeSequences(suffix, kMode));
}

TextFragmentSelector::TextFragmentSelector(SelectorType type,
                                           const String& start,
                                           const String& end,
                                           const String& prefix,
                                           const String& suffix)
    : type_(type), start_(start), end_(end), prefix_(prefix), suffix_(suffix) {}

TextFragmentSelector::TextFragmentSelector(SelectorType type) : type_(type) {}

String TextFragmentSelector::ToString() const {
  StringBuilder selector;
  if (!prefix_.empty()) {
    selector.Append(EscapeSelectorSpecialCharacters(prefix_));
    selector.Append("-,");
  }

  if (!start_.empty()) {
    selector.Append(EscapeSelectorSpecialCharacters(start_));
  }

  if (!end_.empty()) {
    selector.Append(",");
    selector.Append(EscapeSelectorSpecialCharacters(end_));
  }

  if (!suffix_.empty()) {
    selector.Append(",-");
    selector.Append(EscapeSelectorSpecialCharacters(suffix_));
  }

  return selector.ToString();
}

}  // namespace blink
