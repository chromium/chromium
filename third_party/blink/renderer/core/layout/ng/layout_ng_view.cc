// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_view.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/view_fragmentation_context.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "ui/display/screen_info.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#endif

namespace blink {

LayoutNGView::LayoutNGView(ContainerNode* document)
    : LayoutNGBlockFlowMixin<LayoutView>(document) {
  DCHECK(document->IsDocumentNode());

  // This flag is normally set when an object is inserted into the tree, but
  // this doesn't happen for LayoutNGView, since it's the root.
  SetMightTraversePhysicalFragments(true);
}

LayoutNGView::~LayoutNGView() = default;

bool LayoutNGView::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGView ||
         LayoutNGMixin<LayoutView>::IsOfType(type);
}

bool LayoutNGView::IsFragmentationContextRoot() const {
  return ShouldUsePrintingLayout();
}

void LayoutNGView::UpdateLayout() {
  NOT_DESTROYED();
  if (ShouldUsePrintingLayout()) {
    intrinsic_logical_widths_ = LogicalWidth();
    if (!fragmentation_context_) {
      fragmentation_context_ =
          MakeGarbageCollected<ViewFragmentationContext>(*this);
    }
  } else if (fragmentation_context_) {
    fragmentation_context_.Clear();
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // The font code in FontPlatformData does not have a direct connection to the
  // document, the frame or anything from which we could retrieve the device
  // scale factor. After using zoom for DSF, the GraphicsContext does only ever
  // have a DSF of 1 on Linux. In order for the font code to be aware of an up
  // to date DSF when layout happens, we plumb this through to the FontCache, so
  // that we can correctly retrieve RenderStyleForStrike from out of
  // process. crbug.com/845468
  LocalFrame& frame = GetFrameView()->GetFrame();
  ChromeClient& chrome_client = frame.GetChromeClient();
  FontCache::SetDeviceScaleFactor(
      chrome_client.GetScreenInfo(frame).device_scale_factor);
#endif

  bool is_resizing_initial_containing_block =
      LogicalWidth() != ViewLogicalWidthForBoxSizing() ||
      LogicalHeight() != ViewLogicalHeightForBoxSizing();
  bool invalidate_svg_roots =
      GetDocument().SvgExtensions() && !ShouldUsePrintingLayout() &&
      (!GetFrameView() || is_resizing_initial_containing_block);
  if (invalidate_svg_roots) {
    GetDocument()
        .AccessSVGExtensions()
        .InvalidateSVGRootsWithRelativeLengthDescendents();
  }

  DCHECK(!initial_containing_block_resize_handled_list_);
  if (is_resizing_initial_containing_block) {
    initial_containing_block_resize_handled_list_ =
        MakeGarbageCollected<HeapHashSet<Member<const LayoutObject>>>();
  }

  const auto& style = StyleRef();
  NGConstraintSpaceBuilder builder(
      style.GetWritingMode(), style.GetWritingDirection(),
      /* is_new_fc */ true, /* adjust_inline_size_if_needed */ false);
  builder.SetAvailableSize(InitialContainingBlockSize());
  builder.SetIsFixedInlineSize(true);
  builder.SetIsFixedBlockSize(true);

  NGBlockNode(this).Layout(builder.ToConstraintSpace());
  initial_containing_block_resize_handled_list_ = nullptr;
}

AtomicString LayoutNGView::NamedPageAtIndex(wtf_size_t page_index) const {
  // If layout is dirty, it's not possible to look up page names reliably.
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);

  if (!PhysicalFragmentCount())
    return AtomicString();
  DCHECK_EQ(PhysicalFragmentCount(), 1u);
  const NGPhysicalBoxFragment& view_fragment = *GetPhysicalFragment(0);
  const auto children = view_fragment.Children();
  if (page_index >= children.size())
    return AtomicString();
  const auto& page_fragment = To<NGPhysicalBoxFragment>(*children[page_index]);
  return page_fragment.PageName();
}

}  // namespace blink
