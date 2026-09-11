// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_animated_sources.h"

#include "base/check.h"

namespace blink {

void StyleAnimatedSources::Set(AnimatedSourceProperty property,
                               AnimatedSource source) {
  DCHECK(source.IsValid());
  sources_.SetField(property, AnimatedSourceHandle(source.animated_source));

  // Set the bit for untracked dependencies, clearing any previous state.
  const AnimatedSourceBits bit = BitForAnimatedSourceProperty(property);
  if (source.has_untracked_dependencies) {
    has_untracked_dependencies_ |= bit;
  } else {
    has_untracked_dependencies_ &= ~bit;
  }
}

void StyleAnimatedSources::Clear(AnimatedSourceProperty property) {
  sources_.EraseField(property);

  // Reclaim memory for a fully cleared object.
  if (sources_.empty()) {
    sources_.clear();
  }

  // Clear bit for untracked dependencies for the property
  const AnimatedSourceBits bit = BitForAnimatedSourceProperty(property);
  has_untracked_dependencies_ &= ~bit;
}

}  // namespace blink
