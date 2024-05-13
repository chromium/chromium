// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGE_CONTAINER_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGE_CONTAINER_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/layout_algorithm.h"

namespace blink {

class BlockBreakToken;
class BlockNode;
struct PageAreaLayoutParams;

// Algorithm that generates a fragment for a page container, which is
// essentially the containing block of a page (we could refer to it the "margin
// box" of the page, but that would be confusing, since the spec defines up to
// 16 "margin boxes" per page, to hold things like author-generated headers and
// footers).
//
// Inside a page container there are @page margins, borders and padding, and the
// "content box" inside defines the page area, into which fragmented document
// content flows.
//
// See https://drafts.csswg.org/css-page-3/#page-model
//
// The spec has the concept of a "page box". To implement this concept, we
// create two fragments. The page container is the outermost one. In addition to
// any "page margin boxes", the page container contains the other part that
// comprisies the "page box", namely the page border box.
//
// If the destination is an actual printer (and not PDF), The size of the page
// container will always match the selected paper size (whatever @page size
// dictates will be honored by layout, but then scaled down and centered to fit
// on paper).
class CORE_EXPORT PageContainerLayoutAlgorithm
    : public LayoutAlgorithm<BlockNode, BoxFragmentBuilder, BlockBreakToken> {
 public:
  PageContainerLayoutAlgorithm(const LayoutAlgorithmParams& params,
                               wtf_size_t page_index,
                               const AtomicString& page_name,
                               const BlockNode& content_node,
                               const PageAreaLayoutParams&,
                               bool ignore_author_page_style);

  const LayoutResult* Layout();

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) {
    NOTREACHED_NORETURN();
  }

  // Return the outgoing break token from the fragmentainer (page area).
  const BlockBreakToken* FragmentainerBreakToken() const {
    return fragmentainer_break_token_;
  }

 private:
  wtf_size_t page_index_;
  const AtomicString& page_name_;
  const BlockNode& content_node_;
  const PageAreaLayoutParams& page_area_params_;
  bool ignore_author_page_style_;

  const BlockBreakToken* fragmentainer_break_token_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGE_CONTAINER_LAYOUT_ALGORITHM_H_
