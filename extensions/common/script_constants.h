// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_SCRIPT_CONSTANTS_H_
#define EXTENSIONS_COMMON_SCRIPT_CONSTANTS_H_

namespace extensions {

// Whether to fall back to matching the origin for frames where the URL
// cannot be matched directly, such as those with about: or data: schemes.
enum class MatchOriginAsFallbackBehavior {
  // Never fall back on the origin; this means scripts will never match on
  // these frames.
  kNever,
  // Match the origin only for about:-scheme frames, and then climb the frame
  // tree to find an appropriate ancestor to get a full URL (including path).
  // This is for supporting the "match_about_blank" key.
  // TODO(devlin): I wonder if we could simplify this to be "MatchForAbout",
  // and not worry about climbing the frame tree. It would be a behavior
  // change, but I wonder how many extensions it would impact in practice.
  kMatchForAboutSchemeAndClimbTree,
  // Match the origin as a fallback whenever applicable. This won't have a
  // corresponding path.
  kAlways,
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_SCRIPT_CONSTANTS_H_
