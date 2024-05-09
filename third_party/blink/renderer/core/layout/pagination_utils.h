// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGINATION_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGINATION_UTILS_H_

#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class BlockBreakToken;
class LayoutView;
class PhysicalBoxFragment;
struct PhysicalRect;

// Return the total number of pages. Only to be called on a document that has
// been laid out for pagination.
wtf_size_t PageCount(const LayoutView& view);

// Get the page container (BoxType::kPageContainer) for a given page.
const PhysicalBoxFragment* GetPageContainer(const LayoutView&,
                                            wtf_size_t page_number);

// Get the page area (BoxType::kPageArea) for a given page.
const PhysicalBoxFragment* GetPageArea(const LayoutView&,
                                       wtf_size_t page_number);

// Get the page border box (BoxType::kPageBorderBox) child of a page container.
const PhysicalBoxFragment& GetPageBorderBox(
    const PhysicalBoxFragment& page_container);

// Get the page area (BoxType::kPageArea) child of a page border box.
const PhysicalBoxFragment& GetPageArea(
    const PhysicalBoxFragment& page_border_box);

// Return the page rectangle at the specified index in the stitched coordinate
// system.
PhysicalRect StitchedPageContentRect(const LayoutView&, wtf_size_t page_number);

// Return the page rectangle of the page area inside the specified container in
// the stitched coordinate system.
PhysicalRect StitchedPageContentRect(const PhysicalBoxFragment& page_container);

const BlockBreakToken* FindPreviousBreakTokenForPageArea(
    const PhysicalBoxFragment& page_area);

float CalculateOverflowShrinkForPrinting(const LayoutView&,
                                         float maximum_shrink_factor);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_PAGINATION_UTILS_H_
