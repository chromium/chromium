// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_VALID_PROPERTY_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_VALID_PROPERTY_FILTER_H_

namespace blink {

// Some CSS properties do not apply to certain pseudo-elements, and need to be
// ignored when resolving styles. Be aware that these values are used in a
// bitfield. Make sure that it's large enough to hold new values.
// See MatchedProperties::Data::valid_property_filter.
enum class ValidPropertyFilter : unsigned {
  // All properties are valid. This is the common case.
  kNoFilter,
  // Defined in a ::cue pseudo-element scope. Only properties listed
  // in https://w3c.github.io/webvtt/#the-cue-pseudo-element are valid.
  kCue,
  // Defined in a ::first-letter pseudo-element scope. Only properties listed in
  // https://drafts.csswg.org/css-pseudo-4/#first-letter-styling are valid.
  kFirstLetter,
  // Defined in a ::first-line pseudo-element scope. Only properties listed in
  // https://drafts.csswg.org/css-pseudo-4/#first-line-styling are valid.
  kFirstLine,
  // Defined in a ::marker pseudo-element scope. Only properties listed in
  // https://drafts.csswg.org/css-pseudo-4/#marker-pseudo are valid.
  kMarker,
  // Defined in a highlight pseudo-element scope like ::selection and
  // ::target-text. Theoretically only properties listed in
  // https://drafts.csswg.org/css-pseudo-4/#highlight-styling should be valid,
  // but for highlight pseudos using originating inheritance instead of
  // highlight inheritance we allow a different set of rules for
  // compatibility reasons.
  kHighlightLegacy,
  // Defined in a highlight pseudo-element scope like ::selection and
  // ::target-text. Only properties listed in
  // https://drafts.csswg.org/css-pseudo-4/#highlight-styling are valid.
  kHighlight,
  // Defined in a @position-try rule. Only properties listed in
  // https://drafts.csswg.org/css-anchor-position-1/#fallback-rule are valid.
  kPositionTry,
  // Defined in an @page rule.
  // See https://drafts.csswg.org/css-page-3/#page-property-list
  kPageContext,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_VALID_PROPERTY_FILTER_H_
