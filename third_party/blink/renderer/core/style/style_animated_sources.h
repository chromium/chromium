// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_ANIMATED_SOURCES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_ANIMATED_SOURCES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/animated_source_handle.h"
#include "third_party/blink/renderer/core/style/animated_source_properties_bitset.h"
#include "third_party/blink/renderer/platform/sparse_vector.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Element;

// The animating element that drives a property's computed value.
struct CORE_EXPORT AnimatedSource {
  // The element whose animation the computed value is derived from.
  // When the source's computed value changes, this one changes with it.
  AnimatedSourceHandle animated_source;

  // If the value was altered when derived from the source's, so the used value
  // is not an exact copy of the source's used value. (e.g. an inherited
  // `transform` rezoomed by a different effective zoom.)
  bool has_untracked_dependencies = false;

  static AnimatedSource ForElement(Element* element,
                                   bool has_untracked_dependencies = false) {
    return {AnimatedSourceHandle::ForElement(element),
            has_untracked_dependencies};
  }
  bool IsValid() const { return animated_source.IsValid(); }
  bool IsOwnedBy(const Element& element) const {
    return animated_source.IsOwnedBy(element);
  }

  bool operator==(const AnimatedSource&) const = default;
};

// Rare data used by `ComputedStyle` storing the animating element(s) that each
// property's computed value is derived from.
// Stored separately for default inherited and default non-inherited properties,
// so they copy with the values they describe.
class CORE_EXPORT StyleAnimatedSources {
  DISALLOW_NEW();

 public:
  StyleAnimatedSources() = default;
  StyleAnimatedSources(const StyleAnimatedSources&) = default;
  StyleAnimatedSources& operator=(const StyleAnimatedSources&) = default;

  // Returns the source the given property's computed value is from or derived
  // from.
  AnimatedSource Get(AnimatedSourceProperty property) const {
    if (!sources_.HasField(property)) {
      return {};
    }
    const AnimatedSourceBits bit = BitForAnimatedSourceProperty(property);
    return {sources_.GetField(property),
            (has_untracked_dependencies_ & bit) != 0};
  }

  // Writes the source the given property's computed value is from or derived
  // from.
  void Set(AnimatedSourceProperty property, AnimatedSource source);

  // Removes the source for the given property if it exists.
  void Clear(AnimatedSourceProperty property);

  bool operator==(const StyleAnimatedSources&) const = default;

 private:
  // Maps animated property to the animated source element.
  // Inline storage avoids heap allocations for common cases.
  SparseVector<AnimatedSourceProperty, AnimatedSourceHandle, 2> sources_;

  // Which properties have `has_untracked_dependencies` set in `AnimatedSource`.
  AnimatedSourceBits has_untracked_dependencies_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_ANIMATED_SOURCES_H_
