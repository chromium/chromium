// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LINK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LINK_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"

namespace blink {

// We use this struct to store NGLinks in a flexible array in fragments. We have
// to use this struct instead of using NGLink because flexible array members
// cannot have destructors, so we need to do manual refcounting.
struct NGLinkStorage {
  NGPhysicalOffset Offset() const { return offset; }
  const NGPhysicalFragment* get() const { return fragment; }

  operator bool() const { return fragment; }
  const NGPhysicalFragment& operator*() const { return *fragment; }
  const NGPhysicalFragment* operator->() const { return fragment; }

  const NGPhysicalFragment* fragment;
  NGPhysicalOffset offset;
};

// Class representing the offset of a child fragment relative to the
// parent fragment. Fragments themselves have no position information
// allowing entire fragment subtrees to be reused and cached regardless
// of placement.
class CORE_EXPORT NGLink {
  DISALLOW_NEW();

 public:
  NGLink() = default;
  NGLink(scoped_refptr<const NGPhysicalFragment> fragment,
         NGPhysicalOffset offset)
      : fragment_(std::move(fragment)), offset_(offset) {}
  NGLink(NGLink&& o) noexcept
      : fragment_(std::move(o.fragment_)), offset_(o.offset_) {}
  NGLink(const NGLinkStorage& storage)
      : fragment_(storage.fragment), offset_(storage.offset) {}
  ~NGLink() = default;
  NGLink(const NGLink&) = default;
  NGLink& operator=(const NGLink&) = default;
  NGLink& operator=(NGLink&&) = default;

  // Returns the offset relative to the parent fragment's content-box.
  NGPhysicalOffset Offset() const { return offset_; }

  operator bool() const { return fragment_.get(); }
  const NGPhysicalFragment& operator*() const { return *fragment_.get(); }
  const NGPhysicalFragment* operator->() const { return fragment_.get(); }
  const NGPhysicalFragment* get() const { return fragment_.get(); }

 private:
  scoped_refptr<const NGPhysicalFragment> fragment_;
  NGPhysicalOffset offset_;

  // The builder classes needs to set the offset_ field during
  // fragment construciton to allow the child vector to be moved
  // instead of reconstructed during fragment construction.
  friend class NGLayoutResult;
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::NGLink);

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LINK_H_
