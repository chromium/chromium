// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SPELLING_MARKER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SPELLING_MARKER_H_

#include <cstdint>

namespace blink {

class DocumentMarker;

// Represents markers for spelling or grammatical errors in a WebString text.
// Along with the text to be spellchecked, the marker information is necessary
// by the spell check service. Each marker is denoted as a range. `start`
// represents the inclusive beginning index of the range, while `end` represents
// the exclusive end of the range. Both `start` and `end` are measured in UTF-16
// code units.
struct WebSpellingMarker {
 public:
  enum class SpellingMarkerType { kSpelling, kGrammar };
  uint32_t start;
  uint32_t end;
  SpellingMarkerType marker_type;

#ifdef INSIDE_BLINK
  explicit WebSpellingMarker(const DocumentMarker& marker);
#endif  // INSIDE_BLINK
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_SPELLING_MARKER_H_
