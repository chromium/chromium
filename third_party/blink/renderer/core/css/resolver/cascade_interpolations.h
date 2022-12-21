// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_INTERPOLATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_INTERPOLATIONS_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

// bit:  0-15: CSSPropertyID
// bit: 16-23: Entry index
// bit: 24: Presentation attribute bit (inverse)
//
// Our tests currently expect css properties to win over presentation
// attributes. We borrow bit 24 for this purpose, even though it's not really
// part of the position.
inline uint32_t EncodeInterpolationPosition(CSSPropertyID id,
                                            uint8_t index,
                                            bool is_presentation_attribute) {
  static_assert(kIntLastCSSProperty < std::numeric_limits<uint16_t>::max(),
                "Enough bits for CSSPropertyID");
  DCHECK_NE(id, CSSPropertyID::kInvalid);
  DCHECK_LE(id, kLastCSSProperty);
  return (static_cast<uint32_t>(!is_presentation_attribute) << 24) |
         (static_cast<uint32_t>(index & 0xFF) << 16) |
         (static_cast<uint32_t>(id) & 0xFFFF);
}

inline CSSPropertyID DecodeInterpolationPropertyID(uint32_t position) {
  return ConvertToCSSPropertyID(position & 0xFFFF);
}

inline uint8_t DecodeInterpolationIndex(uint32_t position) {
  return (position >> 16) & 0xFF;
}

inline bool DecodeIsPresentationAttribute(uint32_t position) {
  return (~position >> 24) & 1;
}

class CORE_EXPORT CascadeInterpolations {
  STACK_ALLOCATED();

 public:
  static constexpr size_t kMaxEntryIndex = std::numeric_limits<uint8_t>::max();

  struct Entry {
    DISALLOW_NEW();

   public:
    const ActiveInterpolationsMap* map = nullptr;
    CascadeOrigin origin = CascadeOrigin::kNone;
  };

  void Add(const ActiveInterpolationsMap* map, CascadeOrigin origin) {
    DCHECK(map);
    entries_.push_back(Entry{map, origin});
  }

  bool IsEmpty() const { return GetEntries().empty(); }

  const Vector<Entry, 4>& GetEntries() const {
    using EntryVector = Vector<Entry, 4>;
    DEFINE_STATIC_LOCAL(EntryVector, empty, ());
    if (entries_.size() > kMaxEntryIndex + 1) {
      return empty;
    }
    return entries_;
  }

  void Reset() { entries_.clear(); }

 private:
  // We need to add at most four entries (see CSSAnimationUpdate):
  //
  //   1. Standard property transitions
  //   2. Standard property animations
  //   3. Custom property transitions
  //   4. Custom property animations
  //
  // TODO(andruud): Once regular declarations and interpolations are applied
  // using the same StyleCascade object, we can store standard and custom
  // property interpolations together, and use Vector<Entry,2> instead.
  Vector<Entry, 4> entries_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_INTERPOLATIONS_H_
