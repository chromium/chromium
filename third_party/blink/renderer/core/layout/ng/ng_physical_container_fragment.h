// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPhysicalContainerFragment_h
#define NGPhysicalContainerFragment_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "third_party/blink/renderer/core/layout/ng/ng_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGContainerFragmentBuilder;
enum class NGOutlineType;

class CORE_EXPORT NGPhysicalContainerFragment : public NGPhysicalFragment {
 public:
  class ChildLinkList {
   public:
    ChildLinkList(wtf_size_t count, const NGLinkStorage* buffer)
        : count_(count), buffer_(buffer) {}

    wtf_size_t size() const { return count_; }
    const NGLinkStorage& operator[](wtf_size_t idx) const {
      return buffer_[idx];
    }
    const NGLinkStorage& front() const { return buffer_[0]; }
    const NGLinkStorage& back() const { return buffer_[count_ - 1]; }

    const NGLinkStorage* begin() const { return buffer_; }
    const NGLinkStorage* end() const { return begin() + count_; }

    bool IsEmpty() const { return count_ == 0; }

   private:
    wtf_size_t count_;
    const NGLinkStorage* buffer_;
  };

  virtual ChildLinkList Children() const = 0;

  void AddOutlineRectsForNormalChildren(Vector<LayoutRect>* outline_rects,
                                        const LayoutPoint& additional_offset,
                                        NGOutlineType outline_type) const;
  void AddOutlineRectsForDescendant(const NGLink& descendant,
                                    Vector<LayoutRect>* rects,
                                    const LayoutPoint& additional_offset,
                                    NGOutlineType outline_type) const;

 protected:
  // block_or_line_writing_mode is used for converting the child offsets.
  NGPhysicalContainerFragment(NGContainerFragmentBuilder*,
                              WritingMode block_or_line_writing_mode,
                              NGLinkStorage* buffer,
                              NGFragmentType,
                              unsigned sub_type);

  wtf_size_t num_children_;
};

DEFINE_TYPE_CASTS(NGPhysicalContainerFragment,
                  NGPhysicalFragment,
                  fragment,
                  fragment->IsContainer(),
                  fragment.IsContainer());

}  // namespace blink

#endif  // NGPhysicalContainerFragment_h
