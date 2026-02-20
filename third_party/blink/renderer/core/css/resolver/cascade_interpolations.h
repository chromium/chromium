// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_INTERPOLATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_INTERPOLATIONS_H_

#include <array>

#include "base/check_op.h"
#include "third_party/blink/renderer/core/animation/interpolation.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

class CORE_EXPORT CascadeInterpolations {
  STACK_ALLOCATED();

 public:
  struct Entry {
    STACK_ALLOCATED();

   public:
    const ActiveInterpolationsMap* map = nullptr;
    CascadeOrigin origin = CascadeOrigin::kNone;
  };

  void Add(const ActiveInterpolationsMap* map, CascadeOrigin origin) {
    DCHECK(map);
    CHECK_LT(num_entries_, kMaxEntries);
    entries_[num_entries_++] = Entry{map, origin};
  }

  bool IsEmpty() const { return GetEntries().empty(); }

  base::span<const Entry> GetEntries() const {
    return base::span<const Entry>(entries_).first(num_entries_);
  }

  void Reset() { num_entries_ = 0; }

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
  static constexpr size_t kMaxEntries = 4;

  std::array<Entry, kMaxEntries> entries_;
  size_t num_entries_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_INTERPOLATIONS_H_
