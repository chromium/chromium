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
// TODO(mstensho): Add support for @page margins and properties. Inside a page
// container there will be @page margins, borders and padding, and the "content
// box" inside defines the page area, into which fragmented document content
// flows.
//
// See https://drafts.csswg.org/css-page-3/#page-model
//
// The spec has the concept of a "page box". To implement this concept, we
// create two fragments. The page container is the outermost one. In addition to
// any "page margin boxes", the page container contains the other part that
// comprisies the "page box", namely the page border box.
class CORE_EXPORT PageContainerLayoutAlgorithm
    : public LayoutAlgorithm<BlockNode, BoxFragmentBuilder, BlockBreakToken> {
 public:
  PageContainerLayoutAlgorithm(const LayoutAlgorithmParams& params,
                               const BlockNode& content_node,
                               const PageAreaLayoutParams&);

  const LayoutResult* Layout();

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) {
    NOTREACHED_NORETURN();
  }

  // Return the outgoing break token from the fragmentainer (page area).
  const BlockBreakToken* FragmentainerBreakToken() const {
    return fragmentainer_break_token_;
  }

 private:
  const BlockNode& content_node_;
  const PageAreaLayoutParams& page_area_params_;

  const BlockBreakToken* fragmentainer_break_token_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGE_CONTAINER_LAYOUT_ALGORITHM_H_
