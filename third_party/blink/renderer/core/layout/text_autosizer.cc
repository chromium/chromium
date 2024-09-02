/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/text_autosizer.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_column.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

namespace {

inline int GetLayoutInlineSize(const Document& document,
                               const LocalFrameView& main_frame_view) {
  gfx::Size size = main_frame_view.GetLayoutSize();
  return document.GetLayoutView()->IsHorizontalWritingMode() ? size.width()
                                                             : size.height();
}

}  // namespace

static LayoutObject* ParentElementLayoutObject(
    const LayoutObject* layout_object) {
  // At style recalc, the layoutObject's parent may not be attached,
  // so we need to obtain this from the DOM tree.
  const Node* node = layout_object->GetNode();
  if (!node)
    return nullptr;

  // FIXME: This should be using LayoutTreeBuilderTraversal::parent().
  if (Element* parent = node->parentElement())
    return parent->GetLayoutObject();
  return nullptr;
}

static bool IsNonTextAreaFormControl(const LayoutObject* layout_object) {
  const Node* node = layout_object ? layout_object->GetNode() : nullptr;
  const auto* element = DynamicTo<Element>(node);
  if (!element)
    return false;

  return (element->IsFormControlElement() &&
          !IsA<HTMLTextAreaElement>(element));
}

static bool IsPotentialClusterRoot(const LayoutObject* layout_object) {
  // "Potential cluster roots" are the smallest unit for which we can
  // enable/disable text autosizing.
  // - Must have children.
  //   An exception is made for LayoutView which should create a root to
  //   maintain consistency with documents that have no child nodes but may
  //   still have LayoutObject children.
  // - Must not be inline, as different multipliers on one line looks terrible.
  //   Exceptions are inline-block and alike elements (inline-table,
  //   -webkit-inline-*), as they often contain entire multi-line columns of
  //   text.
  // - Must not be normal list items, as items in the same list should look
  //   consistent, unless they are floating or position:absolute/fixed.
  Node* node = layout_object->GeneratingNode();
  if (node && !node->hasChildren() && !IsA<LayoutView>(layout_object))
    return false;
  if (!layout_object->IsLayoutBlock())
    return false;
  if (layout_object->IsInline() &&
      !layout_object->StyleRef().IsDisplayReplacedType())
    return false;
  if (layout_object->IsListItem()) {
    return (layout_object->IsFloating() ||
            layout_object->IsOutOfFlowPositioned());
  }

  return true;
}

static bool IsIndependentDescendant(const LayoutBlock* layout_object) {
  DCHECK(IsPotentialClusterRoot(layout_object));

  LayoutBlock* containing_block = layout_object->ContainingBlock();
  return IsA<LayoutView>(layout_object) || layout_object->IsFloating() ||
         layout_object->IsOutOfFlowPositioned() ||
         layout_object->IsTableCell() || layout_object->IsTableCaption() ||
         layout_object->IsFlexibleBox() ||
         (containing_block && containing_block->IsHorizontalWritingMode() !=
                                  layout_object->IsHorizontalWritingMode()) ||
         layout_object->StyleRef().IsDisplayReplacedType() ||
         layout_object->IsTextArea() ||
         layout_object->StyleRef().UsedUserModify() != EUserModify::kReadOnly;
}

static bool BlockIsRowOfLinks(const LayoutBlock* block) {
  // A "row of links" is a block for which:
  //  1. It does not contain non-link text elements longer than 3 characters
  //  2. It contains a minimum of 3 inline links and all links should
  //     have the same specified font size.
  //  3. It should not contain <br> elements.
  //  4. It should contain only inline elements unless they are containers,
  //     children of link elements or children of sub-containers.
  int link_count = 0;
  LayoutObject* layout_object = block->FirstChild();
  float matching_font_size = -1;

  while (layout_object) {
    if (!IsPotentialClusterRoot(layout_object)) {
      if (layout_object->IsText() &&
          To<LayoutText>(layout_object)
                  ->TransformedText()
                  .LengthWithStrippedWhiteSpace() > 3) {
        return false;
      }
      if (!layout_object->IsInline() || layout_object->IsBR())
        return false;
    }
    if (layout_object->StyleRef().IsLink()) {
      link_count++;
      if (matching_font_size < 0)
        matching_font_size = layout_object->StyleRef().SpecifiedFontSize();
      else if (matching_font_size !=
               layout_object->StyleRef().SpecifiedFontSize())
        return false;

      // Skip traversing descendants of the link.
      layout_object = layout_object->NextInPreOrderAfterChildren(block);
      continue;
    }
    layout_object = layout_object->NextInPreOrder(block);
  }

  return (link_count >= 3);
}

static inline bool HasAnySizingKeyword(const Length& length) {
  return length.HasAutoOrContentOrIntrinsic() || length.HasStretch() ||
         length.IsNone();
}

static bool BlockHeightConstrained(const LayoutBlock* block) {
  // FIXME: Propagate constrainedness down the tree, to avoid inefficiently
  // walking back up from each box.
  // FIXME: This code needs to take into account vertical writing modes.
  // FIXME: Consider additional heuristics, such as ignoring fixed heights if
  // the content is already overflowing before autosizing kicks in.
  for (; block; block = block->ContainingBlock()) {
    const ComputedStyle& style = block->StyleRef();
    if (style.OverflowY() != EOverflow::kVisible &&
        style.OverflowY() != EOverflow::kHidden) {
      return false;
    }
    if (!HasAnySizingKeyword(style.Height()) ||
        !HasAnySizingKeyword(style.MaxHeight()) ||
        block->IsOutOfFlowPositioned()) {
      // Some sites (e.g. wikipedia) set their html and/or body elements to
      // height:100%, without intending to constrain the height of the content
      // within them.
      return !block->IsDocumentElement() && !block->IsBody() &&
             !IsA<LayoutView>(block);
    }
    if (block->IsFloating())
      return false;
  }
  return false;
}

static bool BlockOrImmediateChildrenAreFormControls(const LayoutBlock* block) {
  if (IsNonTextAreaFormControl(block))
    return true;
  const LayoutObject* layout_object = block->FirstChild();
  while (layout_object) {
    if (IsNonTextAreaFormControl(layout_object))
      return true;
    layout_object = layout_object->NextSibling();
  }

  return false;
}

// Some blocks are not autosized even if their parent cluster wants them to.
static bool BlockSuppressesAutosizing(const LayoutBlock* block) {
  if (BlockOrImmediateChildrenAreFormControls(block))
    return true;

  if (BlockIsRowOfLinks(block))
    return true;

  // Don't autosize block-level text that can't wrap (as it's likely to
  // expand sideways and break the page's layout).
  if (!block->StyleRef().ShouldWrapLine()) {
    return true;
  }

  if (BlockHeightConstrained(block))
    return true;

  return false;
}

static bool HasExplicitWidth(const LayoutBlock* block) {
  // FIXME: This heuristic may need to be expanded to other ways a block can be
  // wider or narrower than its parent containing block.
  return block->Style() && !HasAnySizingKeyword(block->StyleRef().Width());
}

static LayoutObject* GetParent(const LayoutObject* object) {
  LayoutObject* parent = nullptr;
  // LayoutObject haven't added to layout tree yet
  if (object->GetNode() && object->GetNode()->parentNode())
    parent = object->GetNode()->parentNode()->GetLayoutObject();
  return parent;
}

TextAutosizer::TextAutosizer(const Document* document)
    : document_(document),
      first_block_to_begin_layout_(nullptr),
#if DCHECK_IS_ON()
      blocks_that_have_begun_layout_(),
#endif
      cluster_stack_(),
      fingerprint_mapper_(),
      page_info_(),
      update_page_info_deferred_(false),
      did_check_cross_site_use_count_(false) {
}

TextAutosizer::~TextAutosizer() = default;

void TextAutosizer::Record(LayoutBlock* block) {
  if (!page_info_.setting_enabled_)
    return;

#if DCHECK_IS_ON()
  DCHECK(!blocks_that_have_begun_layout_.Contains(block));
#endif
  if (!ClassifyBlock(block, INDEPENDENT | EXPLICIT_WIDTH)) {
    // !everHadLayout() means the object hasn't layout yet
    // which means this object is new added.
    // We only deal with new added block here.
    // If parent is new added, no need to check its children.
    LayoutObject* parent = GetParent(block);
    if (!block->EverHadLayout() && parent && parent->EverHadLayout())
      MarkSuperclusterForConsistencyCheck(parent);
    return;
  }

  if (Fingerprint fingerprint = ComputeFingerprint(block))
    fingerprint_mapper_.AddTentativeClusterRoot(block, fingerprint);

  if (!block->EverHadLayout())
    MarkSuperclusterForConsistencyCheck(block);
}

void TextAutosizer::Record(LayoutText* text) {
  if (!text || !ShouldHandleLayout())
    return;
  LayoutObject* parent = GetParent(text);
  if (parent && parent->EverHadLayout())
    MarkSuperclusterForConsistencyCheck(parent);
}

void TextAutosizer::Destroy(LayoutObject* layout_object) {
  if (!page_info_.setting_enabled_ && !fingerprint_mapper_.HasFingerprints())
    return;

#if DCHECK_IS_ON()
  if (layout_object->IsLayoutBlock()) {
    DCHECK(!blocks_that_have_begun_layout_.Contains(
        To<LayoutBlock>(layout_object)));
  }
#endif

  bool result = fingerprint_mapper_.Remove(layout_object);

  if (layout_object->IsLayoutBlock())
    return;

  if (result && first_block_to_begin_layout_) {
    // LayoutBlock with a fingerprint was destroyed during layout.
    // Clear the cluster stack and the supercluster map to avoid stale pointers.
    // Speculative fix for http://crbug.com/369485.
    first_block_to_begin_layout_ = nullptr;
    cluster_stack_.clear();
  }
}

TextAutosizer::BeginLayoutBehavior TextAutosizer::PrepareForLayout(
    LayoutBlock* block) {
#if DCHECK_IS_ON()
  blocks_that_have_begun_layout_.insert(block);
#endif

  if (!first_block_to_begin_layout_) {
    first_block_to_begin_layout_ = block;
    PrepareClusterStack(block->Parent());
    if (IsA<LayoutView>(block))
      CheckSuperclusterConsistency();
  } else if (block == CurrentCluster()->root_) {
    // Ignore beginLayout on the same block twice.
    // This can happen with paginated overflow.
    return kStopLayout;
  }

  return kContinueLayout;
}

void TextAutosizer::PrepareClusterStack(LayoutObject* layout_object) {
  if (!layout_object)
    return;
  PrepareClusterStack(layout_object->Parent());
  if (auto* block = DynamicTo<LayoutBlock>(layout_object)) {
#if DCHECK_IS_ON()
    blocks_that_have_begun_layout_.insert(block);
#endif
    if (Cluster* cluster = MaybeCreateCluster(block))
      cluster_stack_.push_back(cluster);
  }
}

void TextAutosizer::BeginLayout(LayoutBlock* block) {
  DCHECK(ShouldHandleLayout());

  if (PrepareForLayout(block) == kStopLayout)
    return;

  // Skip ruby's inner blocks, because these blocks already are inflated.
  if (block->IsRubyColumn() || block->IsRubyBase() || block->IsRubyText()) {
    return;
  }

  DCHECK(!cluster_stack_.empty() || IsA<LayoutView>(block));
  if (cluster_stack_.empty())
    did_check_cross_site_use_count_ = false;

  if (Cluster* cluster = MaybeCreateCluster(block))
    cluster_stack_.push_back(cluster);

  DCHECK(!cluster_stack_.empty());

  // Cells in auto-layout tables are handled separately by InflateAutoTable.
  auto* cell = DynamicTo<LayoutTableCell>(block);
  bool is_auto_table_cell =
      cell && !cell->Table()->StyleRef().IsFixedTableLayout();
  if (!is_auto_table_cell && !cluster_stack_.empty())
    Inflate(block);
}

void TextAutosizer::InflateAutoTable(LayoutTable* table) {
  DCHECK(table);
  DCHECK(!table->StyleRef().IsFixedTableLayout());
  DCHECK(table->ContainingBlock());

  Cluster* cluster = CurrentCluster();
  if (cluster->root_ != table)
    return;

  // Pre-inflate cells that have enough text so that their inflated preferred
  // widths will be used for column sizing.
  for (LayoutObject* child = table->FirstChild(); child;
       child = child->NextSibling()) {
    auto* section = DynamicTo<LayoutTableSection>(child);
    if (!section) {
      continue;
    }
    for (const LayoutTableRow* row = section->FirstRow(); row;
         row = row->NextRow()) {
      for (LayoutTableCell* cell = row->FirstCell(); cell;
           cell = cell->NextCell()) {
        if (!cell->NeedsLayout()) {
          continue;
        }
        BeginLayout(cell);
        Inflate(cell, kDescendToInnerBlocks);
        EndLayout(cell);
      }
    }
  }
}

void TextAutosizer::EndLayout(LayoutBlock* block) {
  DCHECK(ShouldHandleLayout());

  if (block == first_block_to_begin_layout_) {
    first_block_to_begin_layout_ = nullptr;
    cluster_stack_.clear();
#if DCHECK_IS_ON()
    blocks_that_have_begun_layout_.clear();
#endif
    // Tables can create two layout scopes for the same block so the isEmpty
    // check below is needed to guard against endLayout being called twice.
  } else if (!cluster_stack_.empty() && CurrentCluster()->root_ == block) {
    cluster_stack_.pop_back();
  }
}

float TextAutosizer::Inflate(LayoutObject* parent,
                             InflateBehavior behavior,
                             float multiplier) {
  Cluster* cluster = CurrentCluster();
  bool has_text_child = false;

  LayoutObject* child = nullptr;
  if (parent->IsRuby()) {
    // Skip LayoutRubyColumn which is inline-block.
    // Inflate its inner blocks.
    if (auto* column = DynamicTo<LayoutRubyColumn>(parent->SlowFirstChild())) {
      child = column->FirstChild();
      behavior = kDescendToInnerBlocks;
    }
  } else if (parent->IsLayoutBlock() &&
             (parent->ChildrenInline() || behavior == kDescendToInnerBlocks)) {
    child = To<LayoutBlock>(parent)->FirstChild();
  } else if (parent->IsLayoutInline()) {
    child = To<LayoutInline>(parent)->FirstChild();
  }

  while (child) {
    if (child->IsText()) {
      has_text_child = true;
      // We only calculate this multiplier on-demand to ensure the parent block
      // of this text has entered layout.
      if (!multiplier) {
        multiplier =
            cluster->flags_ & SUPPRESSING ? 1.0f : ClusterMultiplier(cluster);
      }
      ApplyMultiplier(child, multiplier);

      if (behavior == kDescendToInnerBlocks) {
        // The ancestor nodes might be inline-blocks. We should
        // SetIntrinsicLogicalWidthsDirty for ancestor nodes here.
        child->SetIntrinsicLogicalWidthsDirty();
      } else if (parent->IsLayoutInline()) {
        // FIXME: Investigate why MarkOnlyThis is sufficient.
        child->SetIntrinsicLogicalWidthsDirty(kMarkOnlyThis);
      }
    } else if (child->IsLayoutInline()) {
      multiplier = Inflate(child, behavior, multiplier);
      // If this LayoutInline is an anonymous inline that has multiplied
      // children, apply the multiplifer to the parent too. We compute
      // ::first-line style from the style of the parent block.
      if (multiplier && child->IsAnonymous())
        has_text_child = true;
    } else if (child->IsLayoutBlock() && behavior == kDescendToInnerBlocks &&
               !ClassifyBlock(child,
                              INDEPENDENT | EXPLICIT_WIDTH | SUPPRESSING)) {
      multiplier = Inflate(child, behavior, multiplier);
    }
    child = child->NextSibling();
  }

  if (has_text_child) {
    ApplyMultiplier(parent, multiplier);  // Parent handles line spacing.
  } else if (!parent->IsListItem()) {
    // For consistency, a block with no immediate text child should always have
    // a multiplier of 1.
    ApplyMultiplier(parent, 1);
  }

  if (parent->IsLayoutListItem()) {
    float list_item_multiplier = ClusterMultiplier(cluster);
    ApplyMultiplier(parent, list_item_multiplier);

    // The list item has to be treated special because we can have a tree such
    // that you have a list item for a form inside it. The list marker then ends
    // up inside the form and when we try to get the clusterMultiplier we have
    // the wrong cluster root to work from and get the wrong value.
    LayoutObject* marker = To<LayoutListItem>(parent)->Marker();

    // A LayoutOutsideListMarker has a text child that needs its font
    // multiplier updated. Just mark the entire subtree, to make sure we get to
    // it.
    for (LayoutObject* walker = marker; walker;
         walker = walker->NextInPreOrder(marker)) {
      ApplyMultiplier(walker, list_item_multiplier);
      walker->SetIntrinsicLogicalWidthsDirty(kMarkOnlyThis);
    }
  }

  if (page_info_.has_autosized_) {
    document_->CountUse(WebFeature::kTextAutosizing);
    if (page_info_.shared_info_.device_scale_adjustment != 1.0f) {
      document_->CountUse(WebFeature::kUsedDeviceScaleAdjustment);
    }
  }

  return multiplier;
}

bool TextAutosizer::ShouldHandleLayout() const {
  return page_info_.setting_enabled_ && page_info_.page_needs_autosizing_ &&
         !update_page_info_deferred_;
}

bool TextAutosizer::PageNeedsAutosizing() const {
  return page_info_.page_needs_autosizing_;
}

void TextAutosizer::MarkSuperclusterForConsistencyCheck(LayoutObject* object) {
  if (!object || !ShouldHandleLayout())
    return;

  Supercluster* last_supercluster = nullptr;
  while (object) {
    if (auto* block = DynamicTo<LayoutBlock>(object)) {
      if (block->IsTableCell() ||
          ClassifyBlock(block, INDEPENDENT | EXPLICIT_WIDTH)) {
        // If supercluster hasn't been created yet, create one.
        bool is_new_entry = false;
        Supercluster* supercluster =
            fingerprint_mapper_.CreateSuperclusterIfNeeded(block, is_new_entry);
        if (supercluster && supercluster->inherit_parent_multiplier_ ==
                                kDontInheritMultiplier) {
          if (supercluster->has_enough_text_to_autosize_ == kNotEnoughText) {
            fingerprint_mapper_.GetPotentiallyInconsistentSuperclusters()
                .insert(supercluster);
          }
          return;
        }
        if (supercluster &&
            (is_new_entry ||
             supercluster->has_enough_text_to_autosize_ == kNotEnoughText))
          last_supercluster = supercluster;
      }
    }
    object = GetParent(object);
  }

  // If we didn't add any supercluster, we should add one.
  if (last_supercluster) {
    fingerprint_mapper_.GetPotentiallyInconsistentSuperclusters().insert(
        last_supercluster);
  }
}

bool TextAutosizer::HasLayoutInlineSizeChanged() const {
  DCHECK(document_->GetFrame()->IsMainFrame());
  int new_inline_size =
      GetLayoutInlineSize(*document_, *document_->GetFrame()->View());
  return new_inline_size != page_info_.shared_info_.main_frame_layout_width;
}

// Static.
void TextAutosizer::UpdatePageInfoInAllFrames(Frame* main_frame) {
  DCHECK(main_frame && main_frame == main_frame->Tree().Top());
  for (Frame* frame = main_frame; frame; frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;

    Document* document = local_frame->GetDocument();
    // If document is being detached, skip updatePageInfo.
    if (!document || !document->IsActive())
      continue;
    if (TextAutosizer* text_autosizer = document->GetTextAutosizer()) {
      text_autosizer->UpdatePageInfo();

      // Share the page information from the local mainframe with remote ones.
      // TODO(wjmaclean): Refactor this code into a non-static class function
      // called UpdateWebTextAutosizerPageInfoIfNecessary().
      if (frame->IsMainFrame()) {
        const PageInfo& page_info = text_autosizer->page_info_;
        const mojom::blink::TextAutosizerPageInfo& old_page_info =
            document->GetPage()->TextAutosizerPageInfo();
        if (page_info.shared_info_ != old_page_info) {
          document->GetPage()->GetChromeClient().DidUpdateTextAutosizerPageInfo(
              page_info.shared_info_);
          // Remember the RemotePageSettings in the mainframe's renderer so we
          // know when they change.
          document->GetPage()->SetTextAutosizerPageInfo(page_info.shared_info_);
        }
      }
    }
  }
}

void TextAutosizer::UpdatePageInfo() {
  if (update_page_info_deferred_ || !document_->GetPage() ||
      !document_->GetSettings())
    return;

  PageInfo previous_page_info(page_info_);
  page_info_.setting_enabled_ =
      document_->GetSettings()->GetTextAutosizingEnabled();

  if (!page_info_.setting_enabled_ || document_->Printing()) {
    page_info_.page_needs_autosizing_ = false;
  } else {
    auto* layout_view = document_->GetLayoutView();
    bool horizontal_writing_mode =
        IsHorizontalWritingMode(layout_view->StyleRef().GetWritingMode());

    Frame& frame = document_->GetFrame()->Tree().Top();
    if (frame.IsRemoteFrame()) {
      // When the frame is remote, the local main frame is responsible for
      // computing shared_info_ and passing them down to the OOPIF renderers.
      page_info_.shared_info_ = document_->GetPage()->TextAutosizerPageInfo();
    } else {
      LocalFrame& main_frame = To<LocalFrame>(frame);
      gfx::Size frame_size =
          document_->GetSettings()->GetTextAutosizingWindowSizeOverride();
      if (frame_size.IsEmpty())
        frame_size = WindowSize();

      page_info_.shared_info_.main_frame_width =
          horizontal_writing_mode ? frame_size.width() : frame_size.height();

      page_info_.shared_info_.main_frame_layout_width =
          GetLayoutInlineSize(*document_, *main_frame.View());

      // If the page has a meta viewport, don't apply the device scale
      // adjustment.
      if (!main_frame.GetDocument()
               ->GetViewportData()
               .GetViewportDescription()
               .IsSpecifiedByAuthor()) {
        page_info_.shared_info_.device_scale_adjustment =
            document_->GetSettings()->GetDeviceScaleAdjustment();
      } else {
        page_info_.shared_info_.device_scale_adjustment = 1.0f;
      }
    }
    // TODO(pdr): Accessibility should be moved out of the text autosizer.
    // See: crbug.com/645717. We keep the font scale factor available even
    // when the AccessibilityPageZoom feature is enabled so sites that rely on
    // text-size-adjust can still determine the user's desired text scaling.
    page_info_.accessibility_font_scale_factor_ =
        document_->GetSettings()->GetAccessibilityFontScaleFactor();

    // TODO(pdr): pageNeedsAutosizing should take into account whether
    // text-size-adjust is used anywhere on the page because that also needs to
    // trigger autosizing. See: crbug.com/646237.
    page_info_.page_needs_autosizing_ =
        !!page_info_.shared_info_.main_frame_width &&
        (page_info_.accessibility_font_scale_factor_ *
             page_info_.shared_info_.device_scale_adjustment *
             (static_cast<float>(
                  page_info_.shared_info_.main_frame_layout_width) /
              page_info_.shared_info_.main_frame_width) >
         1.0f);
  }

  if (page_info_.page_needs_autosizing_) {
    // If page info has changed, multipliers may have changed. Force a layout to
    // recompute them.
    if (page_info_.shared_info_ != previous_page_info.shared_info_ ||
        page_info_.accessibility_font_scale_factor_ !=
            previous_page_info.accessibility_font_scale_factor_ ||
        page_info_.setting_enabled_ != previous_page_info.setting_enabled_)
      SetAllTextNeedsLayout();
  } else if (previous_page_info.has_autosized_) {
    // If we are no longer autosizing the page, we won't do anything during the
    // next layout. Set all the multipliers back to 1 now.
    ResetMultipliers();
    page_info_.has_autosized_ = false;
  }
}

gfx::Size TextAutosizer::WindowSize() const {
  Page* page = document_->GetPage();
  DCHECK(page);
  return page->GetVisualViewport().Size();
}

void TextAutosizer::ResetMultipliers() {
  LayoutObject* layout_object = document_->GetLayoutView();
  while (layout_object) {
    if (const ComputedStyle* style = layout_object->Style()) {
      if (style->TextAutosizingMultiplier() != 1)
        ApplyMultiplier(layout_object, 1, kLayoutNeeded);
    }
    layout_object = layout_object->NextInPreOrder();
  }
}

void TextAutosizer::SetAllTextNeedsLayout(LayoutBlock* container) {
  if (!container)
    container = document_->GetLayoutView();
  LayoutObject* object = container;
  while (object) {
    if (!object->EverHadLayout()) {
      // Object is new added node, so no need to deal with its children
      object = object->NextInPreOrderAfterChildren(container);
    } else {
      if (object->IsText()) {
        object->SetNeedsLayoutAndFullPaintInvalidation(
            layout_invalidation_reason::kTextAutosizing);
        object->SetNeedsCollectInlines();
      }
      object = object->NextInPreOrder(container);
    }
  }
}

TextAutosizer::BlockFlags TextAutosizer::ClassifyBlock(
    const LayoutObject* layout_object,
    BlockFlags mask) const {
  const auto* block = DynamicTo<LayoutBlock>(layout_object);
  if (!block)
    return 0;

  BlockFlags flags = 0;
  if (IsPotentialClusterRoot(block)) {
    if (mask & POTENTIAL_ROOT)
      flags |= POTENTIAL_ROOT;

    if ((mask & INDEPENDENT) &&
        (IsIndependentDescendant(block) || block->IsTable() ||
         block->StyleRef().SpecifiesColumns()))
      flags |= INDEPENDENT;

    if ((mask & EXPLICIT_WIDTH) && HasExplicitWidth(block))
      flags |= EXPLICIT_WIDTH;

    if ((mask & SUPPRESSING) && BlockSuppressesAutosizing(block))
      flags |= SUPPRESSING;
  }
  return flags;
}

bool TextAutosizer::ClusterWouldHaveEnoughTextToAutosize(
    const LayoutBlock* root,
    const LayoutBlock* width_provider) {
  Cluster* hypothetical_cluster =
      MakeGarbageCollected<Cluster>(root, ClassifyBlock(root), nullptr);
  return ClusterHasEnoughTextToAutosize(hypothetical_cluster, width_provider);
}

bool TextAutosizer::ClusterHasEnoughTextToAutosize(
    Cluster* cluster,
    const LayoutBlock* width_provider) {
  if (cluster->has_enough_text_to_autosize_ != kUnknownAmountOfText)
    return cluster->has_enough_text_to_autosize_ == kHasEnoughText;

  const LayoutBlock* root = cluster->root_;
  if (!width_provider)
    width_provider = ClusterWidthProvider(root);

  // TextAreas and user-modifiable areas get a free pass to autosize regardless
  // of text content.
  if (root->IsTextArea() ||
      (root->Style() &&
       root->StyleRef().UsedUserModify() != EUserModify::kReadOnly)) {
    cluster->has_enough_text_to_autosize_ = kHasEnoughText;
    return true;
  }

  if (cluster->flags_ & SUPPRESSING) {
    cluster->has_enough_text_to_autosize_ = kNotEnoughText;
    return false;
  }

  // 4 lines of text is considered enough to autosize.
  float minimum_text_length_to_autosize = WidthFromBlock(width_provider) * 4;
  if (LocalFrame* frame = document_->GetFrame()) {
    minimum_text_length_to_autosize /=
        document_->GetPage()->GetChromeClient().WindowToViewportScalar(frame,
                                                                       1);
  }

  float length = 0;
  LayoutObject* descendant = root->FirstChild();
  while (descendant) {
    if (descendant->IsLayoutBlock()) {
      if (ClassifyBlock(descendant, INDEPENDENT | SUPPRESSING)) {
        descendant = descendant->NextInPreOrderAfterChildren(root);
        continue;
      }
    } else if (descendant->IsText()) {
      // Note: Using text().LengthWithStrippedWhiteSpace() instead of
      // resolvedTextLength() because the lineboxes will not be built until
      // layout. These values can be different.
      // Note: This is an approximation assuming each character is 1em wide.
      length += To<LayoutText>(descendant)
                    ->TransformedText()
                    .LengthWithStrippedWhiteSpace() *
                descendant->StyleRef().SpecifiedFontSize();

      if (length >= minimum_text_length_to_autosize) {
        cluster->has_enough_text_to_autosize_ = kHasEnoughText;
        return true;
      }
    }
    descendant = descendant->NextInPreOrder(root);
  }

  cluster->has_enough_text_to_autosize_ = kNotEnoughText;
  return false;
}

TextAutosizer::Fingerprint TextAutosizer::GetFingerprint(
    LayoutObject* layout_object) {
  Fingerprint result = fingerprint_mapper_.Get(layout_object);
  if (!result) {
    result = ComputeFingerprint(layout_object);
    fingerprint_mapper_.Add(layout_object, result);
  }
  return result;
}

TextAutosizer::Fingerprint TextAutosizer::ComputeFingerprint(
    const LayoutObject* layout_object) {
  auto* element = DynamicTo<Element>(layout_object->GeneratingNode());
  if (!element)
    return 0;

  FingerprintSourceData data;
  if (LayoutObject* parent = ParentElementLayoutObject(layout_object))
    data.parent_hash_ = GetFingerprint(parent);

  data.qualified_name_hash_ = WTF::GetHash(element->TagQName());

  if (const ComputedStyle* style = layout_object->Style()) {
    data.packed_style_properties_ = static_cast<unsigned>(style->Direction());
    data.packed_style_properties_ |=
        (static_cast<unsigned>(style->GetPosition()) << 1);
    data.packed_style_properties_ |=
        (static_cast<unsigned>(style->UnresolvedFloating()) << 4);
    data.packed_style_properties_ |=
        (static_cast<unsigned>(style->Display()) << 7);
    const Length& width = style->Width();
    data.packed_style_properties_ |= (width.GetType() << 12);
    // packedStyleProperties effectively using 16 bits now.

    // TODO(kojii): The width can be computed from style only when it's fixed.
    // consider for adding: writing mode, padding.
    data.width_ = width.IsFixed() ? width.GetFloatValue() : .0f;
  }

  // Use nodeIndex as a rough approximation of column number
  // (it's too early to call LayoutTableCell::col).
  // FIXME: account for colspan
  if (layout_object->IsTableCell())
    data.column_ = layout_object->GetNode()->NodeIndex();

  return StringHasher::HashMemory(&data, sizeof(data));
}

TextAutosizer::Cluster* TextAutosizer::MaybeCreateCluster(LayoutBlock* block) {
  BlockFlags flags = ClassifyBlock(block);
  if (!(flags & POTENTIAL_ROOT))
    return nullptr;

  Cluster* parent_cluster = cluster_stack_.empty() ? nullptr : CurrentCluster();
  DCHECK(parent_cluster || IsA<LayoutView>(block));

  // If a non-independent block would not alter the SUPPRESSING flag, it doesn't
  // need to be a cluster.
  bool parent_suppresses =
      parent_cluster && (parent_cluster->flags_ & SUPPRESSING);
  if (!(flags & INDEPENDENT) && !(flags & EXPLICIT_WIDTH) &&
      !!(flags & SUPPRESSING) == parent_suppresses)
    return nullptr;

  bool is_new_entry = false;
  Cluster* cluster = MakeGarbageCollected<Cluster>(
      block, flags, parent_cluster,
      fingerprint_mapper_.CreateSuperclusterIfNeeded(block, is_new_entry));
  return cluster;
}

TextAutosizer::Supercluster*
TextAutosizer::FingerprintMapper::CreateSuperclusterIfNeeded(
    LayoutBlock* block,
    bool& is_new_entry) {
  Fingerprint fingerprint = Get(block);
  if (!fingerprint)
    return nullptr;

  BlockSet* roots = GetTentativeClusterRoots(fingerprint);
  if (!roots || roots->size() < 2 || !roots->Contains(block))
    return nullptr;

  SuperclusterMap::AddResult add_result =
      superclusters_.insert(fingerprint, nullptr);
  is_new_entry = add_result.is_new_entry;
  if (!add_result.is_new_entry)
    return add_result.stored_value->value.Get();

  Supercluster* supercluster = MakeGarbageCollected<Supercluster>(roots);
  add_result.stored_value->value = supercluster;
  return supercluster;
}

float TextAutosizer::ClusterMultiplier(Cluster* cluster) {
  if (cluster->multiplier_)
    return cluster->multiplier_;

  // FIXME: why does isWiderOrNarrowerDescendant crash on independent clusters?
  if (!(cluster->flags_ & INDEPENDENT) && IsWiderOrNarrowerDescendant(cluster))
    cluster->flags_ |= WIDER_OR_NARROWER;

  if (cluster->flags_ & (INDEPENDENT | WIDER_OR_NARROWER)) {
    if (cluster->supercluster_) {
      cluster->multiplier_ = SuperclusterMultiplier(cluster);
      cluster->supercluster_->inherit_parent_multiplier_ =
          kDontInheritMultiplier;
    } else if (ClusterHasEnoughTextToAutosize(cluster)) {
      cluster->multiplier_ =
          MultiplierFromBlock(ClusterWidthProvider(cluster->root_));
    } else {
      cluster->multiplier_ = 1.0f;
    }
  } else {
    cluster->multiplier_ =
        cluster->parent_ ? ClusterMultiplier(cluster->parent_) : 1.0f;
    if (cluster->supercluster_)
      cluster->supercluster_->inherit_parent_multiplier_ = kInheritMultiplier;
  }

  DCHECK(cluster->multiplier_);
  return cluster->multiplier_;
}

bool TextAutosizer::SuperclusterHasEnoughTextToAutosize(
    Supercluster* supercluster,
    const LayoutBlock* width_provider,
    const bool skip_layouted_nodes) {
  if (supercluster->has_enough_text_to_autosize_ != kUnknownAmountOfText)
    return supercluster->has_enough_text_to_autosize_ == kHasEnoughText;

  for (const auto& root : *supercluster->roots_) {
    if (skip_layouted_nodes && !root->ChildNeedsFullLayout()) {
      continue;
    }
    if (ClusterWouldHaveEnoughTextToAutosize(root, width_provider)) {
      supercluster->has_enough_text_to_autosize_ = kHasEnoughText;
      return true;
    }
  }
  supercluster->has_enough_text_to_autosize_ = kNotEnoughText;
  return false;
}

float TextAutosizer::SuperclusterMultiplier(Cluster* cluster) {
  Supercluster* supercluster = cluster->supercluster_;
  if (!supercluster->multiplier_) {
    const LayoutBlock* width_provider =
        MaxClusterWidthProvider(cluster->supercluster_, cluster->root_);
    CHECK(width_provider);
    supercluster->multiplier_ =
        SuperclusterHasEnoughTextToAutosize(supercluster, width_provider, false)
            ? MultiplierFromBlock(width_provider)
            : 1.0f;
  }
  DCHECK(supercluster->multiplier_);
  return supercluster->multiplier_;
}

const LayoutBlock* TextAutosizer::ClusterWidthProvider(
    const LayoutBlock* root) const {
  if (root->IsTable() || root->IsTableCell())
    return root;

  return DeepestBlockContainingAllText(root);
}

const LayoutBlock* TextAutosizer::MaxClusterWidthProvider(
    Supercluster* supercluster,
    const LayoutBlock* current_root) const {
  const LayoutBlock* result = nullptr;
  if (current_root)
    result = ClusterWidthProvider(current_root);

  float max_width = 0;
  if (result)
    max_width = WidthFromBlock(result);

  const BlockSet* roots = supercluster->roots_;
  for (const auto& root : *roots) {
    const LayoutBlock* width_provider = ClusterWidthProvider(root);
    if (width_provider->NeedsLayout())
      continue;
    float width = WidthFromBlock(width_provider);
    if (width > max_width) {
      max_width = width;
      result = width_provider;
    }
  }
  return result;
}

float TextAutosizer::WidthFromBlock(const LayoutBlock* block) const {
  CHECK(block);
  CHECK(block->Style());

  if (!(block->IsTable() || block->IsTableCell() || block->IsListItem())) {
    return ContentInlineSize(block);
  }

  if (!block->ContainingBlock())
    return 0;

  // Tables may be inflated before computing their preferred widths. Try several
  // methods to obtain a width, and fall back on a containing block's width.
  for (; block; block = block->ContainingBlock()) {
    float width;
    Length specified_width = block->StyleRef().LogicalWidth();
    if (specified_width.IsFixed()) {
      if ((width = specified_width.Value()) > 0)
        return width;
    }
    if (specified_width.HasPercent()) {
      if (float container_width = ContentInlineSize(block->ContainingBlock())) {
        if ((width = FloatValueForLength(specified_width, container_width)) > 0)
          return width;
      }
    }
    if ((width = ContentInlineSize(block)) > 0)
      return width;
  }
  return 0;
}

float TextAutosizer::MultiplierFromBlock(const LayoutBlock* block) {
// If block->needsLayout() is false, it does not need to be in
// m_blocksThatHaveBegunLayout. This can happen during layout of a positioned
// object if the cluster's DBCAT is deeper than the positioned object's
// containing block, and wasn't marked as needing layout.
#if DCHECK_IS_ON()
  DCHECK(blocks_that_have_begun_layout_.Contains(block) ||
         !block->NeedsLayout() || IsA<LayoutMultiColumnFlowThread>(block));
#endif
  // Block width, in CSS pixels.
  float block_width = WidthFromBlock(block);
  float layout_width = std::min(
      block_width,
      static_cast<float>(page_info_.shared_info_.main_frame_layout_width));
  float multiplier =
      page_info_.shared_info_.main_frame_width
          ? layout_width / page_info_.shared_info_.main_frame_width
          : 1.0f;
  multiplier *= page_info_.accessibility_font_scale_factor_ *
                page_info_.shared_info_.device_scale_adjustment;
  return std::max(multiplier, 1.0f);
}

const LayoutBlock* TextAutosizer::DeepestBlockContainingAllText(
    Cluster* cluster) {
  if (!cluster->deepest_block_containing_all_text_)
    cluster->deepest_block_containing_all_text_ =
        DeepestBlockContainingAllText(cluster->root_);

  return cluster->deepest_block_containing_all_text_.Get();
}

// FIXME: Refactor this to look more like TextAutosizer::deepestCommonAncestor.
const LayoutBlock* TextAutosizer::DeepestBlockContainingAllText(
    const LayoutBlock* root) const {
  // To avoid font-size shaking caused by the change of LayoutView's
  // DeepestBlockContainingAllText.
  if (IsA<LayoutView>(root))
    return root;

  size_t first_depth = 0;
  const LayoutObject* first_text_leaf = FindTextLeaf(root, first_depth, kFirst);
  if (!first_text_leaf)
    return root;

  size_t last_depth = 0;
  const LayoutObject* last_text_leaf = FindTextLeaf(root, last_depth, kLast);
  DCHECK(last_text_leaf);

  // Equalize the depths if necessary. Only one of the while loops below will
  // get executed.
  const LayoutObject* first_node = first_text_leaf;
  const LayoutObject* last_node = last_text_leaf;
  while (first_depth > last_depth) {
    first_node = first_node->Parent();
    --first_depth;
  }
  while (last_depth > first_depth) {
    last_node = last_node->Parent();
    --last_depth;
  }

  // Go up from both nodes until the parent is the same. Both pointers will
  // point to the LCA then.
  while (first_node != last_node) {
    first_node = first_node->Parent();
    last_node = last_node->Parent();
  }

  if (auto* layout_block = DynamicTo<LayoutBlock>(first_node))
    return layout_block;

  // containingBlock() should never leave the cluster, since it only skips
  // ancestors when finding the container of position:absolute/fixed blocks, and
  // those cannot exist between a cluster and its text node's lowest common
  // ancestor as isAutosizingCluster would have made them into their own
  // independent cluster.
  const LayoutBlock* containing_block = first_node->ContainingBlock();
  if (!containing_block)
    return root;

  DCHECK(containing_block->IsDescendantOf(root));
  return containing_block;
}

const LayoutObject* TextAutosizer::FindTextLeaf(
    const LayoutObject* parent,
    size_t& depth,
    TextLeafSearch first_or_last) const {
  // List items are treated as text due to the marker.
  if (parent->IsListItem()) {
    return parent;
  }

  if (parent->IsText())
    return parent;

  ++depth;
  const LayoutObject* child = (first_or_last == kFirst)
                                  ? parent->SlowFirstChild()
                                  : parent->SlowLastChild();
  while (child) {
    // Note: At this point clusters may not have been created for these blocks
    // so we cannot rely on m_clusters. Instead, we use a best-guess about
    // whether the block will become a cluster.
    if (!ClassifyBlock(child, INDEPENDENT)) {
      if (const LayoutObject* leaf = FindTextLeaf(child, depth, first_or_last))
        return leaf;
    }
    child = (first_or_last == kFirst) ? child->NextSibling()
                                      : child->PreviousSibling();
  }
  --depth;

  return nullptr;
}

static bool IsCrossSite(const Frame& frame1, const Frame& frame2) {
  // Cross-site differs from cross-origin. For example, http://foo.com and
  // http://sub.foo.com are cross-origin but same-site. Only cross-site text
  // autosizing is impacted by site isolation (crbug.com/393285).

  const auto* origin1 = frame1.GetSecurityContext()->GetSecurityOrigin();
  const auto* origin2 = frame2.GetSecurityContext()->GetSecurityOrigin();
  if (!origin1 || !origin2 || origin1->CanAccess(origin2))
    return false;

  if (origin1->Protocol() != origin2->Protocol())
    return true;

  // Compare eTLD+1.
  return network_utils::GetDomainAndRegistry(
             origin1->Host(), network_utils::kIncludePrivateRegistries) !=
         network_utils::GetDomainAndRegistry(
             origin2->Host(), network_utils::kIncludePrivateRegistries);
}

void TextAutosizer::ReportIfCrossSiteFrame() {
  LocalFrame* frame = document_->GetFrame();
  LocalFrameView* view = document_->View();
  if (!frame || !view || !view->IsAttached() || !view->IsVisible() ||
      view->Size().IsEmpty() || !IsCrossSite(*frame, frame->Tree().Top()))
    return;

  document_->CountUse(WebFeature::kTextAutosizedCrossSiteIframe);
}

void TextAutosizer::ApplyMultiplier(LayoutObject* layout_object,
                                    float multiplier,
                                    RelayoutBehavior relayout_behavior) {
  DCHECK(layout_object);
  const ComputedStyle& current_style = layout_object->StyleRef();
  if (!current_style.GetTextSizeAdjust().IsAuto()) {
    if (RuntimeEnabledFeatures::TextSizeAdjustImprovementsEnabled()) {
      // Non-auto values of text-size-adjust should fully disable automatic
      // text size adjustment, including the accessibility font scale factor.
      multiplier = 1;
    } else {
      // The accessibility font scale factor is applied by the autosizer so we
      // need to apply that scale factor on top of the text-size-adjust
      // multiplier. Only apply the accessibility factor if the autosizer has
      // determined a multiplier should be applied so that text-size-adjust:none
      // does not cause a multiplier to be applied when it wouldn't be
      // otherwise.
      bool should_apply_accessibility_font_scale_factor = multiplier > 1;
      multiplier = current_style.GetTextSizeAdjust().Multiplier();
      if (should_apply_accessibility_font_scale_factor) {
        multiplier *= page_info_.accessibility_font_scale_factor_;
      }
    }
  } else if (multiplier < 1) {
    // Unlike text-size-adjust, the text autosizer should only inflate fonts.
    multiplier = 1;
  }

  if (current_style.TextAutosizingMultiplier() == multiplier)
    return;

  ComputedStyleBuilder builder(current_style);
  builder.SetTextAutosizingMultiplier(multiplier);
  const ComputedStyle* style = builder.TakeStyle();

  if (multiplier > 1 && !did_check_cross_site_use_count_) {
    ReportIfCrossSiteFrame();
    did_check_cross_site_use_count_ = true;
  }

  switch (relayout_behavior) {
    case kAlreadyInLayout:
      layout_object->SetModifiedStyleOutsideStyleRecalc(
          style, LayoutObject::ApplyStyleChanges::kNo);
      if (layout_object->IsText())
        To<LayoutText>(layout_object)->AutosizingMultiplerChanged();
      layout_object->SetNeedsLayoutAndFullPaintInvalidation(
          layout_invalidation_reason::kTextAutosizing, kMarkContainerChain);
      break;

    case kLayoutNeeded:
      layout_object->SetModifiedStyleOutsideStyleRecalc(
          style, LayoutObject::ApplyStyleChanges::kYes);
      break;
  }

  if (multiplier != 1)
    page_info_.has_autosized_ = true;
}

bool TextAutosizer::IsWiderOrNarrowerDescendant(Cluster* cluster) {
  // FIXME: Why do we return true when hasExplicitWidth returns false??
  if (!cluster->parent_ || !HasExplicitWidth(cluster->root_))
    return true;

  const LayoutBlock* parent_deepest_block_containing_all_text =
      DeepestBlockContainingAllText(cluster->parent_);
#if DCHECK_IS_ON()
  DCHECK(blocks_that_have_begun_layout_.Contains(cluster->root_));
  DCHECK(blocks_that_have_begun_layout_.Contains(
      parent_deepest_block_containing_all_text));
#endif

  float content_width =
      ContentInlineSize(DeepestBlockContainingAllText(cluster));
  float cluster_text_width =
      ContentInlineSize(parent_deepest_block_containing_all_text);

  // Clusters with a root that is wider than the deepestBlockContainingAllText
  // of their parent autosize independently of their parent.
  if (content_width > cluster_text_width)
    return true;

  // Clusters with a root that is significantly narrower than the
  // deepestBlockContainingAllText of their parent autosize independently of
  // their parent.
  constexpr float kNarrowWidthDifference = 200;
  if (cluster_text_width - content_width > kNarrowWidthDifference)
    return true;

  return false;
}

void TextAutosizer::Supercluster::Trace(Visitor* visitor) const {
  visitor->Trace(roots_);
}

TextAutosizer::Cluster* TextAutosizer::CurrentCluster() const {
  SECURITY_DCHECK(!cluster_stack_.empty());
  return cluster_stack_.back().Get();
}

TextAutosizer::Cluster::Cluster(const LayoutBlock* root,
                                BlockFlags flags,
                                Cluster* parent,
                                Supercluster* supercluster)
    : root_(root),
      flags_(flags),
      deepest_block_containing_all_text_(nullptr),
      parent_(parent),
      multiplier_(0),
      has_enough_text_to_autosize_(kUnknownAmountOfText),
      supercluster_(supercluster),
      has_table_ancestor_(root->IsTableCell() ||
                          (parent_ && parent_->has_table_ancestor_)) {}

void TextAutosizer::Cluster::Trace(Visitor* visitor) const {
  visitor->Trace(root_);
  visitor->Trace(deepest_block_containing_all_text_);
  visitor->Trace(parent_);
  visitor->Trace(supercluster_);
}

#if DCHECK_IS_ON()
void TextAutosizer::FingerprintMapper::AssertMapsAreConsistent() {
  // For each fingerprint -> block mapping in m_blocksForFingerprint we should
  // have an associated map from block -> fingerprint in m_fingerprints.
  ReverseFingerprintMap::iterator end = blocks_for_fingerprint_.end();
  for (ReverseFingerprintMap::iterator fingerprint_it =
           blocks_for_fingerprint_.begin();
       fingerprint_it != end; ++fingerprint_it) {
    Fingerprint fingerprint = fingerprint_it->key;
    BlockSet* blocks = fingerprint_it->value;
    for (auto& block : *blocks)
      DCHECK_EQ(fingerprints_.at(block), fingerprint);
  }
}
#endif

void TextAutosizer::FingerprintMapper::Add(LayoutObject* layout_object,
                                           Fingerprint fingerprint) {
  Remove(layout_object);

  fingerprints_.Set(layout_object, fingerprint);
#if DCHECK_IS_ON()
  AssertMapsAreConsistent();
#endif
}

void TextAutosizer::FingerprintMapper::AddTentativeClusterRoot(
    LayoutBlock* block,
    Fingerprint fingerprint) {
  Add(block, fingerprint);

  ReverseFingerprintMap::AddResult add_result =
      blocks_for_fingerprint_.insert(fingerprint, nullptr);
  if (add_result.is_new_entry)
    add_result.stored_value->value = MakeGarbageCollected<BlockSet>();
  add_result.stored_value->value->insert(block);
#if DCHECK_IS_ON()
  AssertMapsAreConsistent();
#endif
}

bool TextAutosizer::FingerprintMapper::Remove(LayoutObject* layout_object) {
  Fingerprint fingerprint = fingerprints_.Take(layout_object);
  if (!fingerprint || !layout_object->IsLayoutBlock())
    return false;

  ReverseFingerprintMap::iterator blocks_iter =
      blocks_for_fingerprint_.find(fingerprint);
  if (blocks_iter == blocks_for_fingerprint_.end())
    return false;

  BlockSet& blocks = *blocks_iter->value;
  blocks.erase(To<LayoutBlock>(layout_object));
  if (blocks.empty()) {
    blocks_for_fingerprint_.erase(blocks_iter);

    SuperclusterMap::iterator supercluster_iter =
        superclusters_.find(fingerprint);

    if (supercluster_iter != superclusters_.end()) {
      Supercluster* supercluster = supercluster_iter->value;
      potentially_inconsistent_superclusters_.erase(supercluster);
      superclusters_.erase(supercluster_iter);
    }
  }
#if DCHECK_IS_ON()
  AssertMapsAreConsistent();
#endif
  return true;
}

TextAutosizer::Fingerprint TextAutosizer::FingerprintMapper::Get(
    const LayoutObject* layout_object) {
  auto it = fingerprints_.find(layout_object);
  return it != fingerprints_.end() ? it->value : TextAutosizer::Fingerprint();
}

TextAutosizer::BlockSet*
TextAutosizer::FingerprintMapper::GetTentativeClusterRoots(
    Fingerprint fingerprint) {
  auto it = blocks_for_fingerprint_.find(fingerprint);
  return it != blocks_for_fingerprint_.end() ? &*it->value : nullptr;
}

TextAutosizer::LayoutScope::LayoutScope(LayoutBlock* block)
    : text_autosizer_(block->GetDocument().GetTextAutosizer()), block_(block) {
  if (!text_autosizer_)
    return;

  if (text_autosizer_->ShouldHandleLayout())
    text_autosizer_->BeginLayout(block_);
  else
    text_autosizer_ = nullptr;
}

TextAutosizer::LayoutScope::~LayoutScope() {
  if (text_autosizer_)
    text_autosizer_->EndLayout(block_);
}

TextAutosizer::TableLayoutScope::TableLayoutScope(LayoutTable* table)
    : LayoutScope(table) {
  if (text_autosizer_) {
    DCHECK(text_autosizer_->ShouldHandleLayout());
    text_autosizer_->InflateAutoTable(table);
  }
}

TextAutosizer::DeferUpdatePageInfo::DeferUpdatePageInfo(Page* page)
    : main_frame_(page->DeprecatedLocalMainFrame()) {
  // TODO(wjmaclean): see if we need to try and extend deferred updates to
  // renderers for remote main frames or not. For now, it's safe to assume
  // main_frame_ will be local, see WebViewImpl::ResizeViewWhileAnchored().
  DCHECK(main_frame_);
  if (TextAutosizer* text_autosizer =
          main_frame_->GetDocument()->GetTextAutosizer()) {
    DCHECK(!text_autosizer->update_page_info_deferred_);
    text_autosizer->update_page_info_deferred_ = true;
  }
}

// static
void TextAutosizer::MaybeRegisterInlineSize(const LayoutBlock& ng_block,
                                            LayoutUnit inline_size) {
  if (auto* text_autosizer = ng_block.GetDocument().GetTextAutosizer()) {
    if (text_autosizer->ShouldHandleLayout())
      text_autosizer->RegisterInlineSize(ng_block, inline_size);
  }
}

TextAutosizer::NGLayoutScope::NGLayoutScope(LayoutBox* box,
                                            LayoutUnit inline_size)
    : text_autosizer_(box->GetDocument().GetTextAutosizer()),
      block_(DynamicTo<LayoutBlock>(box)) {
  // Bail if:
  //  - Text autosizing isn't enabled.
  //  - If the chid isn't a LayoutBlock.
  //  - If the child is a LayoutOutsideListMarker. (They are super-small
  //    blocks, and using them to determine if we should autosize the text will
  //    typically false, overriding whatever its parent has already correctly
  //    determined).
  if (!text_autosizer_ || !text_autosizer_->ShouldHandleLayout() || !block_ ||
      block_->IsLayoutOutsideListMarker()) {
    text_autosizer_ = nullptr;
    return;
  }

  // In order for the text autosizer to do anything useful at all, it needs to
  // know the inline size of the block. So register it. LayoutNG normally
  // writes back to the legacy tree *after* layout, but this one must be ready
  // before, at least if the autosizer is enabled.
  text_autosizer_->RegisterInlineSize(*block_, inline_size);

  text_autosizer_->BeginLayout(block_);
}

TextAutosizer::NGLayoutScope::~NGLayoutScope() {
  if (text_autosizer_) {
    text_autosizer_->EndLayout(block_);
    text_autosizer_->UnregisterInlineSize(*block_);
  }
}

TextAutosizer::DeferUpdatePageInfo::~DeferUpdatePageInfo() {
  if (TextAutosizer* text_autosizer =
          main_frame_->GetDocument()->GetTextAutosizer()) {
    DCHECK(text_autosizer->update_page_info_deferred_);
    text_autosizer->update_page_info_deferred_ = false;
    TextAutosizer::UpdatePageInfoInAllFrames(main_frame_);
  }
}

float TextAutosizer::ComputeAutosizedFontSize(float computed_size,
                                              float multiplier,
                                              float effective_zoom) {
  DCHECK_GE(multiplier, 0);

  // Somewhat arbitrary "pleasant" font size.
  const float kPleasantSize = 16 * effective_zoom;

  // Multiply fonts that the page author has specified to be larger than
  // pleasantSize by less and less, until huge fonts are not increased at all.
  // For specifiedSize between 0 and pleasantSize we directly apply the
  // multiplier; hence for specifiedSize == pleasantSize, computedSize will be
  // multiplier * pleasantSize. For greater specifiedSizes we want to
  // gradually fade out the multiplier, so for every 1px increase in
  // specifiedSize beyond pleasantSize we will only increase computedSize
  // by gradientAfterPleasantSize px until we meet the
  // computedSize = specifiedSize line, after which we stay on that line (so
  // then every 1px increase in specifiedSize increases computedSize by 1px).
  const float kGradientAfterPleasantSize = 0.5;

  float auto_sized_size;
  // Skip linear backoff for multipliers that shrink the size or when the font
  // sizes are small.
  if (multiplier <= 1 || computed_size <= kPleasantSize) {
    auto_sized_size = multiplier * computed_size;
  } else {
    auto_sized_size =
        multiplier * kPleasantSize +
        kGradientAfterPleasantSize * (computed_size - kPleasantSize);
    if (auto_sized_size < computed_size)
      auto_sized_size = computed_size;
  }
  return auto_sized_size;
}

void TextAutosizer::CheckSuperclusterConsistency() {
  HeapHashSet<Member<Supercluster>>& potentially_inconsistent_superclusters =
      fingerprint_mapper_.GetPotentiallyInconsistentSuperclusters();
  if (potentially_inconsistent_superclusters.empty())
    return;

  for (Supercluster* supercluster : potentially_inconsistent_superclusters) {
    if (kHasEnoughText == supercluster->has_enough_text_to_autosize_)
      continue;

    float old_multipiler = supercluster->multiplier_;
    supercluster->multiplier_ = 0;
    supercluster->has_enough_text_to_autosize_ = kUnknownAmountOfText;
    const LayoutBlock* width_provider =
        MaxClusterWidthProvider(supercluster, nullptr);
    if (!width_provider)
      continue;

    if (SuperclusterHasEnoughTextToAutosize(supercluster, width_provider,
                                            true) == kHasEnoughText) {
      for (const auto& root : *supercluster->roots_) {
        if (!root->EverHadLayout())
          continue;

        DCHECK(root);
        SetAllTextNeedsLayout(root);
      }
    } else {
      supercluster->multiplier_ = old_multipiler;
    }
  }
  potentially_inconsistent_superclusters.clear();
}

float TextAutosizer::ContentInlineSize(const LayoutBlock* block) const {
  if (!block->IsLayoutNGObject())
    return block->ContentLogicalWidth().ToFloat();
  auto iter = inline_size_map_.find(block);
  if (iter == inline_size_map_.end())
    return block->ContentLogicalWidth().ToFloat();
  LayoutUnit size = iter.Get()->value;
  if (block->IsHorizontalWritingMode()) {
    size = block->ClientWidthFrom(size) - block->PaddingLeft() -
           block->PaddingRight();
  } else {
    size = block->ClientHeightFrom(size) - block->PaddingTop() -
           block->PaddingBottom();
  }
  return size.ClampNegativeToZero().ToFloat();
}

void TextAutosizer::RegisterInlineSize(const LayoutBlock& ng_block,
                                       LayoutUnit inline_size) {
  inline_size_map_.insert(&ng_block, inline_size);
}

void TextAutosizer::UnregisterInlineSize(const LayoutBlock& ng_block) {
  inline_size_map_.erase(&ng_block);
}

void TextAutosizer::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(first_block_to_begin_layout_);
  visitor->Trace(inline_size_map_);
#if DCHECK_IS_ON()
  visitor->Trace(blocks_that_have_begun_layout_);
#endif
  visitor->Trace(cluster_stack_);
  visitor->Trace(fingerprint_mapper_);
}

void TextAutosizer::FingerprintMapper::Trace(Visitor* visitor) const {
  visitor->Trace(fingerprints_);
  visitor->Trace(blocks_for_fingerprint_);
  visitor->Trace(superclusters_);
  visitor->Trace(potentially_inconsistent_superclusters_);
}

}  // namespace blink
