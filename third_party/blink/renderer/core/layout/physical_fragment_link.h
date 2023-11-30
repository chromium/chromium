// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PHYSICAL_FRAGMENT_LINK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PHYSICAL_FRAGMENT_LINK_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"

namespace blink {

class PhysicalFragment;

// Class representing the offset of a child fragment relative to the
// parent fragment. Fragments themselves have no position information
// allowing entire fragment subtrees to be reused and cached regardless
// of placement.
// This class is stored in a C-style regular array on
// PhysicalFragment. It cannot have destructors. Fragment reference
// counting is done manually.
struct CORE_EXPORT PhysicalFragmentLink {
  DISALLOW_NEW();

 public:
  PhysicalOffset Offset() const { return offset; }
  const PhysicalFragment* get() const { return fragment.Get(); }

  explicit operator bool() const { return fragment != nullptr; }
  const PhysicalFragment& operator*() const { return *fragment; }
  const PhysicalFragment* operator->() const { return fragment.Get(); }

  void Trace(Visitor* visitor) const { visitor->Trace(fragment); }

  Member<const PhysicalFragment> fragment;
  PhysicalOffset offset;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::PhysicalFragmentLink)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PHYSICAL_FRAGMENT_LINK_H_
