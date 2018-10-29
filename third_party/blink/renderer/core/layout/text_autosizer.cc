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
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/layout_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_ruby_run.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"

namespace blink {

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
  if (!node || !node->IsElementNode())
    return false;
  const Element* element = ToElement(node);

  return (element->IsFormControlElement() && !IsHTMLTextAreaElement(element));
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
  if (node && !node->hasChildren() && !layout_object->IsLayoutView())
    return false;
  if (!layout_object->IsLayoutBlock())
    return false;
  if (layout_object->IsInline() &&
      !layout_object->StyleRef().IsDisplayReplacedType())
    return false;
  if (layout_object->IsListItemIncludingNG())
    return (layout_object->IsFloating() ||
            layout_object->IsOutOfFlowPositioned());

  return true;
}

static bool IsIndependentDescendant(const LayoutBlock* layout_object) {
  DCHECK(IsPotentialClusterRoot(layout_object));

  LayoutBlock* containing_block = layout_object->ContainingBlock();
  return layout_object->IsLayoutView() || layout_object->IsFloating() ||
         layout_object->IsOutOfFlowPositioned() ||
         layout_object->IsTableCell() || layout_object->IsTableCaption() ||
         layout_object->IsFlexibleBoxIncludingDeprecated() ||
         (containing_block && containing_block->IsHorizontalWritingMode() !=
                                  layout_object->IsHorizontalWritingMode()) ||
         layout_object->StyleRef().IsDisplayReplacedType() ||
         layout_object->IsTextArea() ||
         layout_object->StyleRef().UserModify() != EUserModify::kReadOnly;
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
          ToLayoutText(layout_object)->GetText().StripWhiteSpace().length() > 3)
        return false;
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

static bool BlockHeightConstrained(const LayoutBlock* block) {
  // FIXME: Propagate constrainedness down the tree, to avoid inefficiently
  // walking back up from each box.
  // FIXME: This code needs to take into account vertical writing modes.
  // FIXME: Consider additional heuristics, such as ignoring fixed heights if
  // the content is already overflowing before autosizing kicks in.
  for (; block; block = block->ContainingBlock()) {
    const ComputedStyle& style = block->StyleRef();
    if (style.OverflowY() != EOverflow::kVisible
        && style.OverflowY() != EOverflow::kHidden)
      return false;
    if (style.Height().IsSpecified() || style.MaxHeight().IsSpecified() ||
        block->IsOutOfFlowPositioned()) {
      // Some sites (e.g. wikipedia) set their html and/or body elements to
      // height:100%, without intending to constrain the height of the content
      // within them.
      return !block->IsDocumentElement() && !block->IsBody() &&
             !block->IsLayoutView();
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
  if (!block->StyleRef().AutoWrap())
    return true;

  if (BlockHeightConstrained(block))
    return true;

  return false;
}

static bool HasExplicitWidth(const LayoutBlock* block) {
  // FIXME: This heuristic may need to be expanded to other ways a block can be
  // wider or narrower than its parent containing block.
  return block->Style() && block->StyleRef().Width().IsSpecified();
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
      update_page_info_deferred_(false) {
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

void TextAutosizer::Destroy(LayoutBlock* block) {
  if (!page_info_.setting_enabled_ && !fingerprint_mapper_.HasFingerprints())
    return;

#if DCHECK_IS_ON()
  DCHECK(!blocks_that_have_begun_layout_.Contains(block));
#endif

  if (fingerprint_mapper_.Remove(block) && first_block_to_begin_layout_) {
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
    if (block->IsLayoutView())
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

  if (layout_object->IsLayoutBlock()) {
    LayoutBlock* block = ToLayoutBlock(layout_object);
#if DCHECK_IS_ON()
    blocks_that_have_begun_layout_.insert(block);
#endif
    if (Cluster* cluster = MaybeCreateCluster(block))
      cluster_stack_.push_back(base::WrapUnique(cluster));
  }
}

void TextAutosizer::BeginLayout(LayoutBlock* block,
                                SubtreeLayoutScope* layouter) {
  DCHECK(ShouldHandleLayout());

  if (PrepareForLayout(block) == kStopLayout)
    return;

  // Skip ruby's inner blocks, because these blocks already are inflated.
  if (block->IsRubyRun() || block->IsRubyBase() || block->IsRubyText())
    return;

  DCHECK(!cluster_stack_.IsEmpty() || block->IsLayoutView());

  if (Cluster* cluster = MaybeCreateCluster(block))
    cluster_stack_.push_back(base::WrapUnique(cluster));

  DCHECK(!cluster_stack_.IsEmpty());

  // Cells in auto-layout tables are handled separately by inflateAutoTable.
  bool is_auto_table_cell =
      block->IsTableCell() &&
      !ToLayoutTableCell(block)->Table()->StyleRef().IsFixedTableLayout();
  if (!is_auto_table_cell && !cluster_stack_.IsEmpty())
    Inflate(block, layouter);
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
  for (LayoutObject* section = table->FirstChild(); section;
       section = section->NextSibling()) {
    if (!section->IsTableSection())
      continue;
    for (LayoutTableRow* row = ToLayoutTableSection(section)->FirstRow(); row;
         row = row->NextRow()) {
      for (LayoutTableCell* cell = row->FirstCell(); cell;
           cell = cell->NextCell()) {
        if (!cell->NeedsLayout())
          continue;

        BeginLayout(cell, nullptr);
        Inflate(cell, nullptr, kDescendToInnerBlocks);
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
    styles_retained_during_layout_.clear();
#if DCHECK_IS_ON()
    blocks_that_have_begun_layout_.clear();
#endif
    // Tables can create two layout scopes for the same block so the isEmpty
    // check below is needed to guard against endLayout being called twice.
  } else if (!cluster_stack_.IsEmpty() && CurrentCluster()->root_ == block) {
    cluster_stack_.pop_back();
  }
}

float TextAutosizer::Inflate(LayoutObject* parent,
                             SubtreeLayoutScope* layouter,
                             InflateBehavior behavior,
                             float multiplier) {
  Cluster* cluster = CurrentCluster();
  bool has_text_child = false;

  LayoutObject* child = nullptr;
  if (parent->IsRuby()) {
    // Skip layoutRubyRun which is inline-block.
    // Inflate rubyRun's inner blocks.
    LayoutObject* run = parent->SlowFirstChild();
    if (run && run->IsRubyRun()) {
      child = ToLayoutRubyRun(run)->FirstChild();
      behavior = kDescendToInnerBlocks;
    }
  } else if (parent->IsLayoutBlock() &&
             (parent->ChildrenInline() || behavior == kDescendToInnerBlocks)) {
    child = ToLayoutBlock(parent)->FirstChild();
  } else if (parent->IsLayoutInline()) {
    child = ToLayoutInline(parent)->FirstChild();
  }

  while (child) {
    if (child->IsText()) {
      has_text_child = true;
      // We only calculate this multiplier on-demand to ensure the parent block
      // of this text has entered layout.
      if (!multiplier)
        multiplier =
            cluster->flags_ & SUPPRESSING ? 1.0f : ClusterMultiplier(cluster);
      ApplyMultiplier(child, multiplier, layouter);

      if (behavior == kDescendToInnerBlocks) {
        // The ancestor nodes might be inline-blocks. We should
        // setPreferredLogicalWidthsDirty for ancestor nodes here.
        child->SetPreferredLogicalWidthsDirty();
      } else if (parent->IsLayoutInline()) {
        // FIXME: Investigate why MarkOnlyThis is sufficient.
        child->SetPreferredLogicalWidthsDirty(kMarkOnlyThis);
      }
    } else if (child->IsLayoutInline()) {
      multiplier = Inflate(child, layouter, behavior, multiplier);
    } else if (child->IsLayoutBlock() && behavior == kDescendToInnerBlocks &&
               !ClassifyBlock(child,
                              INDEPENDENT | EXPLICIT_WIDTH | SUPPRESSING)) {
      multiplier = Inflate(child, layouter, behavior, multiplier);
    }
    child = child->NextSibling();
  }

  if (has_text_child) {
    ApplyMultiplier(parent, multiplier,
                    layouter);  // Parent handles line spacing.
  } else if (!parent->IsListItemIncludingNG()) {
    // For consistency, a block with no immediate text child should always have
    // a multiplier of 1.
    ApplyMultiplier(parent, 1, layouter);
  }

  if (parent->IsListItemIncludingNG()) {
    float multiplier = ClusterMultiplier(cluster);
    ApplyMultiplier(parent, multiplier, layouter);

    // The list item has to be treated special because we can have a tree such
    // that you have a list item for a form inside it. The list marker then ends
    // up inside the form and when we try to get the clusterMultiplier we have
    // the wrong cluster root to work from and get the wrong value.
    LayoutObject* marker = nullptr;
    if (parent->IsListItem())
      marker = ToLayoutListItem(parent)->Marker();
    else if (parent->IsLayoutNGListItem())
      marker = ToLayoutNGListItem(parent)->Marker();

    // A LayoutNGListMarker has a text child that needs its font multiplier
    // updated. Just mark the entire subtree, to make sure we get to it.
    for (LayoutObject* walker = marker; walker;
         walker = walker->NextInPreOrder(marker)) {
      ApplyMultiplier(walker, multiplier, layouter);
      walker->SetPreferredLogicalWidthsDirty(kMarkOnlyThis);
    }
  }

  if (page_info_.has_autosized_)
    UseCounter::Count(*document_, WebFeature::kTextAutosizing);

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
  LayoutBlock* block = nullptr;
  while (object) {
    if (object->IsLayoutBlock()) {
      block = ToLayoutBlock(object);
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

void TextAutosizer::UpdatePageInfoInAllFrames() {
  DCHECK(!document_->GetFrame() || document_->GetFrame()->IsMainFrame());

  for (Frame* frame = document_->GetFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (!frame->IsLocalFrame())
      continue;

    Document* document = ToLocalFrame(frame)->GetDocument();
    // If document is being detached, skip updatePageInfo.
    if (!document || !document->IsActive())
      continue;
    if (TextAutosizer* text_autosizer = document->GetTextAutosizer())
      text_autosizer->UpdatePageInfo();
  }
}

void TextAutosizer::UpdatePageInfo() {
  if (update_page_info_deferred_ || !document_->GetPage() ||
      !document_->GetSettings())
    return;

  PageInfo previous_page_info(page_info_);
  page_info_.setting_enabled_ =
      document_->GetSettings()->TextAutosizingEnabled();

  if (!page_info_.setting_enabled_ || document_->Printing()) {
    page_info_.page_needs_autosizing_ = false;
  } else {
    auto* layout_view = document_->GetLayoutView();
    bool horizontal_writing_mode =
        IsHorizontalWritingMode(layout_view->StyleRef().GetWritingMode());

    // FIXME: With out-of-process iframes, the top frame can be remote and
    // doesn't have sizing information. Just return if this is the case.
    Frame& frame = document_->GetFrame()->Tree().Top();
    if (frame.IsRemoteFrame())
      return;

    LocalFrame& main_frame = ToLocalFrame(frame);
    IntSize frame_size =
        document_->GetSettings()->TextAutosizingWindowSizeOverride();
    if (frame_size.IsEmpty())
      frame_size = WindowSize();

    page_info_.frame_width_ =
        horizontal_writing_mode ? frame_size.Width() : frame_size.Height();

    IntSize layout_size = main_frame.View()->GetLayoutSize();
    page_info_.layout_width_ =
        horizontal_writing_mode ? layout_size.Width() : layout_size.Height();

    // TODO(pdr): Accessibility should be moved out of the text autosizer. See:
    // crbug.com/645717.
    page_info_.accessibility_font_scale_factor_ =
        document_->GetSettings()->GetAccessibilityFontScaleFactor();

    // If the page has a meta viewport or @viewport, don't apply the device
    // scale adjustment.
    if (!main_frame.GetDocument()
             ->GetViewportData()
             .GetViewportDescription()
             .IsSpecifiedByAuthor()) {
      page_info_.device_scale_adjustment_ =
          document_->GetSettings()->GetDeviceScaleAdjustment();
    } else {
      page_info_.device_scale_adjustment_ = 1.0f;
    }

    // TODO(pdr): pageNeedsAutosizing should take into account whether
    // text-size-adjust is used anywhere on the page because that also needs to
    // trigger autosizing. See: crbug.com/646237.
    page_info_.page_needs_autosizing_ =
        !!page_info_.frame_width_ &&
        (page_info_.accessibility_font_scale_factor_ *
             page_info_.device_scale_adjustment_ *
             (static_cast<float>(page_info_.layout_width_) /
              page_info_.frame_width_) >
         1.0f);
  }

  if (page_info_.page_needs_autosizing_) {
    // If page info has changed, multipliers may have changed. Force a layout to
    // recompute them.
    if (page_info_.frame_width_ != previous_page_info.frame_width_ ||
        page_info_.layout_width_ != previous_page_info.layout_width_ ||
        page_info_.accessibility_font_scale_factor_ !=
            previous_page_info.accessibility_font_scale_factor_ ||
        page_info_.device_scale_adjustment_ !=
            previous_page_info.device_scale_adjustment_ ||
        page_info_.setting_enabled_ != previous_page_info.setting_enabled_)
      SetAllTextNeedsLayout();
  } else if (previous_page_info.has_autosized_) {
    // If we are no longer autosizing the page, we won't do anything during the
    // next layout. Set all the multipliers back to 1 now.
    ResetMultipliers();
    page_info_.has_autosized_ = false;
  }
}

IntSize TextAutosizer::WindowSize() const {
  Page* page = document_->GetPage();
  DCHECK(page);
  return page->GetVisualViewport().Size();
}

void TextAutosizer::ResetMultipliers() {
  LayoutObject* layout_object = document_->GetLayoutView();
  while (layout_object) {
    if (const ComputedStyle* style = layout_object->Style()) {
      if (style->TextAutosizingMultiplier() != 1)
        ApplyMultiplier(layout_object, 1, nullptr, kLayoutNeeded);
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
            LayoutInvalidationReason::kTextAutosizing);
      }
      object = object->NextInPreOrder(container);
    }
  }
}

TextAutosizer::BlockFlags TextAutosizer::ClassifyBlock(
    const LayoutObject* layout_object,
    BlockFlags mask) const {
  if (!layout_object->IsLayoutBlock())
    return 0;

  const LayoutBlock* block = ToLayoutBlock(layout_object);
  BlockFlags flags = 0;

  if (IsPotentialClusterRoot(block)) {
    if (mask & POTENTIAL_ROOT)
      flags |= POTENTIAL_ROOT;

    LayoutMultiColumnFlowThread* flow_thread = nullptr;
    if (block->IsLayoutBlockFlow())
      flow_thread = ToLayoutBlockFlow(block)->MultiColumnFlowThread();
    if ((mask & INDEPENDENT) &&
        (IsIndependentDescendant(block) || block->IsTable() ||
         (flow_thread && flow_thread->ColumnCount() > 1)))
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
  Cluster hypothetical_cluster(root, ClassifyBlock(root), nullptr);
  return ClusterHasEnoughTextToAutosize(&hypothetical_cluster, width_provider);
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
  if (root->IsTextArea() || (root->Style() && root->StyleRef().UserModify() !=
                                                  EUserModify::kReadOnly)) {
    cluster->has_enough_text_to_autosize_ = kHasEnoughText;
    return true;
  }

  if (cluster->flags_ & SUPPRESSING) {
    cluster->has_enough_text_to_autosize_ = kNotEnoughText;
    return false;
  }

  // 4 lines of text is considered enough to autosize.
  float minimum_text_length_to_autosize = WidthFromBlock(width_provider) * 4;
  if (LocalFrameView* view = document_->View()) {
    minimum_text_length_to_autosize =
        document_->GetPage()
            ->GetChromeClient()
            .ViewportToScreen(IntRect(0, 0, minimum_text_length_to_autosize, 0),
                              view)
            .Width();
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
      // Note: Using text().stripWhiteSpace().length() instead of
      // resolvedTextLength() because the lineboxes will not be built until
      // layout. These values can be different.
      // Note: This is an approximation assuming each character is 1em wide.
      length += ToLayoutText(descendant)->GetText().StripWhiteSpace().length() *
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
  Node* node = layout_object->GeneratingNode();
  if (!node || !node->IsElementNode())
    return 0;

  FingerprintSourceData data;
  if (LayoutObject* parent = ParentElementLayoutObject(layout_object))
    data.parent_hash_ = GetFingerprint(parent);

  data.qualified_name_hash_ =
      QualifiedNameHash::GetHash(ToElement(node)->TagQName());

  if (const ComputedStyle* style = layout_object->Style()) {
    data.packed_style_properties_ = static_cast<unsigned>(style->Direction());
    data.packed_style_properties_ |=
        (static_cast<unsigned>(style->GetPosition()) << 1);
    data.packed_style_properties_ |=
        (static_cast<unsigned>(style->Floating()) << 4);
    data.packed_style_properties_ |=
        (static_cast<unsigned>(style->Display()) << 6);
    data.packed_style_properties_ |= (style->Width().GetType() << 11);
    // packedStyleProperties effectively using 15 bits now.

    // consider for adding: writing mode, padding.

    data.width_ = style->Width().GetFloatValue();
  }

  // Use nodeIndex as a rough approximation of column number
  // (it's too early to call LayoutTableCell::col).
  // FIXME: account for colspan
  if (layout_object->IsTableCell())
    data.column_ = layout_object->GetNode()->NodeIndex();

  return StringHasher::ComputeHash<UChar>(
      static_cast<const UChar*>(static_cast<const void*>(&data)),
      sizeof data / sizeof(UChar));
}

TextAutosizer::Cluster* TextAutosizer::MaybeCreateCluster(LayoutBlock* block) {
  BlockFlags flags = ClassifyBlock(block);
  if (!(flags & POTENTIAL_ROOT))
    return nullptr;

  Cluster* parent_cluster =
      cluster_stack_.IsEmpty() ? nullptr : CurrentCluster();
  DCHECK(parent_cluster || block->IsLayoutView());

  // If a non-independent block would not alter the SUPPRESSING flag, it doesn't
  // need to be a cluster.
  bool parent_suppresses =
      parent_cluster && (parent_cluster->flags_ & SUPPRESSING);
  if (!(flags & INDEPENDENT) && !(flags & EXPLICIT_WIDTH) &&
      !!(flags & SUPPRESSING) == parent_suppresses)
    return nullptr;

  bool is_new_entry = false;
  Cluster* cluster = new Cluster(
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
      superclusters_.insert(fingerprint, std::unique_ptr<Supercluster>());
  is_new_entry = add_result.is_new_entry;
  if (!add_result.is_new_entry)
    return add_result.stored_value->value.get();

  Supercluster* supercluster = new Supercluster(roots);
  add_result.stored_value->value = base::WrapUnique(supercluster);
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
    } else if (ClusterHasEnoughTextToAutosize(cluster))
      cluster->multiplier_ =
          MultiplierFromBlock(ClusterWidthProvider(cluster->root_));
    else
      cluster->multiplier_ = 1.0f;
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

  for (auto* root : *supercluster->roots_) {
    if (skip_layouted_nodes && !root->NormalChildNeedsLayout())
      continue;
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
  for (const auto* root : *roots) {
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

  if (!(block->IsTable() || block->IsTableCell() ||
        block->IsListItemIncludingNG()))
    return block->ContentLogicalWidth().ToFloat();

  if (!block->ContainingBlock())
    return 0;

  // Tables may be inflated before computing their preferred widths. Try several
  // methods to obtain a width, and fall back on a containing block's width.
  for (; block; block = block->ContainingBlock()) {
    float width;
    Length specified_width =
        block->IsTableCell()
            ? ToLayoutTableCell(block)->StyleOrColLogicalWidth()
            : block->StyleRef().LogicalWidth();
    if (specified_width.IsFixed()) {
      if ((width = specified_width.Value()) > 0)
        return width;
    }
    if (specified_width.IsPercentOrCalc()) {
      if (float container_width =
              block->ContainingBlock()->ContentLogicalWidth().ToFloat()) {
        if ((width = FloatValueForLength(specified_width, container_width)) > 0)
          return width;
      }
    }
    if ((width = block->ContentLogicalWidth().ToFloat()) > 0)
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
         !block->NeedsLayout());
#endif
  // Block width, in CSS pixels.
  float block_width = WidthFromBlock(block);
  float layout_width =
      std::min(block_width, static_cast<float>(page_info_.layout_width_));
  float multiplier =
      page_info_.frame_width_ ? layout_width / page_info_.frame_width_ : 1.0f;
  multiplier *= page_info_.accessibility_font_scale_factor_ *
                page_info_.device_scale_adjustment_;
  return std::max(multiplier, 1.0f);
}

const LayoutBlock* TextAutosizer::DeepestBlockContainingAllText(
    Cluster* cluster) {
  if (!cluster->deepest_block_containing_all_text_)
    cluster->deepest_block_containing_all_text_ =
        DeepestBlockContainingAllText(cluster->root_);

  return cluster->deepest_block_containing_all_text_;
}

// FIXME: Refactor this to look more like TextAutosizer::deepestCommonAncestor.
const LayoutBlock* TextAutosizer::DeepestBlockContainingAllText(
    const LayoutBlock* root) const {
  // To avoid font-size shaking caused by the change of LayoutView's
  // DeepestBlockContainingAllText.
  if (root->IsLayoutView())
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

  if (first_node->IsLayoutBlock())
    return ToLayoutBlock(first_node);

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
  if (parent->IsListItemIncludingNG())
    return parent;

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

void TextAutosizer::ApplyMultiplier(LayoutObject* layout_object,
                                    float multiplier,
                                    SubtreeLayoutScope* layouter,
                                    RelayoutBehavior relayout_behavior) {
  DCHECK(layout_object);
  const ComputedStyle& current_style = layout_object->StyleRef();
  if (!current_style.GetTextSizeAdjust().IsAuto()) {
    // The accessibility font scale factor is applied by the autosizer so we
    // need to apply that scale factor on top of the text-size-adjust
    // multiplier. Only apply the accessibility factor if the autosizer has
    // determined a multiplier should be applied so that text-size-adjust:none
    // does not cause a multiplier to be applied when it wouldn't be otherwise.
    bool should_apply_accessibility_font_scale_factor = multiplier > 1;
    multiplier = current_style.GetTextSizeAdjust().Multiplier();
    if (should_apply_accessibility_font_scale_factor)
      multiplier *= page_info_.accessibility_font_scale_factor_;
  } else if (multiplier < 1) {
    // Unlike text-size-adjust, the text autosizer should only inflate fonts.
    multiplier = 1;
  }

  if (current_style.TextAutosizingMultiplier() == multiplier)
    return;

  scoped_refptr<ComputedStyle> style = ComputedStyle::Clone(current_style);
  style->SetTextAutosizingMultiplier(multiplier);

  switch (relayout_behavior) {
    case kAlreadyInLayout:
      // Don't free currentStyle until the end of the layout pass. This allows
      // other parts of the system to safely hold raw ComputedStyle* pointers
      // during layout, e.g. BreakingContext::m_currentStyle.
      styles_retained_during_layout_.push_back(&current_style);

      layout_object->SetStyleInternal(std::move(style));
      if (layout_object->IsText())
        ToLayoutText(layout_object)->AutosizingMultiplerChanged();
      DCHECK(!layouter || layout_object->IsDescendantOf(&layouter->Root()));
      layout_object->SetNeedsLayoutAndFullPaintInvalidation(
          LayoutInvalidationReason::kTextAutosizing, kMarkContainerChain,
          layouter);
      layout_object->MarkContainerNeedsCollectInlines();
      break;

    case kLayoutNeeded:
      DCHECK(!layouter);
      layout_object->SetStyle(std::move(style));
      break;
  }

  if (multiplier != 1)
    page_info_.has_autosized_ = true;

  layout_object->ClearBaseComputedStyle();
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
      DeepestBlockContainingAllText(cluster)->ContentLogicalWidth().ToFloat();
  float cluster_text_width =
      parent_deepest_block_containing_all_text->ContentLogicalWidth().ToFloat();

  // Clusters with a root that is wider than the deepestBlockContainingAllText
  // of their parent autosize independently of their parent.
  if (content_width > cluster_text_width)
    return true;

  // Clusters with a root that is significantly narrower than the
  // deepestBlockContainingAllText of their parent autosize independently of
  // their parent.
  static float narrow_width_difference = 200;
  if (cluster_text_width - content_width > narrow_width_difference)
    return true;

  return false;
}

TextAutosizer::Cluster* TextAutosizer::CurrentCluster() const {
  SECURITY_DCHECK(!cluster_stack_.IsEmpty());
  return cluster_stack_.back().get();
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

#if DCHECK_IS_ON()
void TextAutosizer::FingerprintMapper::AssertMapsAreConsistent() {
  // For each fingerprint -> block mapping in m_blocksForFingerprint we should
  // have an associated map from block -> fingerprint in m_fingerprints.
  ReverseFingerprintMap::iterator end = blocks_for_fingerprint_.end();
  for (ReverseFingerprintMap::iterator fingerprint_it =
           blocks_for_fingerprint_.begin();
       fingerprint_it != end; ++fingerprint_it) {
    Fingerprint fingerprint = fingerprint_it->key;
    BlockSet* blocks = fingerprint_it->value.get();
    for (BlockSet::iterator block_it = blocks->begin();
         block_it != blocks->end(); ++block_it) {
      const LayoutBlock* block = (*block_it);
      DCHECK_EQ(fingerprints_.at(block), fingerprint);
    }
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
      blocks_for_fingerprint_.insert(fingerprint, std::unique_ptr<BlockSet>());
  if (add_result.is_new_entry)
    add_result.stored_value->value = std::make_unique<BlockSet>();
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
  blocks.erase(ToLayoutBlock(layout_object));
  if (blocks.IsEmpty()) {
    blocks_for_fingerprint_.erase(blocks_iter);

    SuperclusterMap::iterator supercluster_iter =
        superclusters_.find(fingerprint);

    if (supercluster_iter != superclusters_.end()) {
      Supercluster* supercluster = supercluster_iter->value.get();
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
  return fingerprints_.at(layout_object);
}

TextAutosizer::BlockSet*
TextAutosizer::FingerprintMapper::GetTentativeClusterRoots(
    Fingerprint fingerprint) {
  return blocks_for_fingerprint_.at(fingerprint);
}

TextAutosizer::LayoutScope::LayoutScope(LayoutBlock* block,
                                        SubtreeLayoutScope* layouter)
    : text_autosizer_(block->GetDocument().GetTextAutosizer()), block_(block) {
  if (!text_autosizer_)
    return;

  if (text_autosizer_->ShouldHandleLayout())
    text_autosizer_->BeginLayout(block_, layouter);
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
  if (TextAutosizer* text_autosizer =
          main_frame_->GetDocument()->GetTextAutosizer()) {
    DCHECK(!text_autosizer->update_page_info_deferred_);
    text_autosizer->update_page_info_deferred_ = true;
  }
}

TextAutosizer::NGLayoutScope::NGLayoutScope(const NGBlockNode& node,
                                            LayoutUnit inline_size)
    : text_autosizer_(node.GetLayoutBox()->GetDocument().GetTextAutosizer()),
      block_(ToLayoutBlockFlow(node.GetLayoutBox())) {
  if (!text_autosizer_ || !text_autosizer_->ShouldHandleLayout() ||
      block_->IsLayoutNGListMarker()) {
    // Bail if text autosizing isn't enabled, but also if this is a
    // IsLayoutNGListMarker. They are super-small blocks, and using them to
    // determine if we should autosize the text will typically always yield
    // false, overriding whatever its parent (typically the list item) has
    // already correctly determined.
    text_autosizer_ = nullptr;
    return;
  }

  // In order for the text autosizer to do anything useful at all, it needs to
  // know the inline size of the block. So set it. LayoutNG normally writes back
  // to the legacy tree *after* layout, but this one must be set before, at
  // least if the autosizer is enabled.
  block_->SetLogicalWidth(inline_size);

  text_autosizer_->BeginLayout(block_, nullptr);
}

TextAutosizer::NGLayoutScope::~NGLayoutScope() {
  if (text_autosizer_)
    text_autosizer_->EndLayout(block_);
}

TextAutosizer::DeferUpdatePageInfo::~DeferUpdatePageInfo() {
  if (TextAutosizer* text_autosizer =
          main_frame_->GetDocument()->GetTextAutosizer()) {
    DCHECK(text_autosizer->update_page_info_deferred_);
    text_autosizer->update_page_info_deferred_ = false;
    text_autosizer->UpdatePageInfoInAllFrames();
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
  HashSet<Supercluster*>& potentially_inconsistent_superclusters =
      fingerprint_mapper_.GetPotentiallyInconsistentSuperclusters();
  if (potentially_inconsistent_superclusters.IsEmpty())
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
      for (auto* root : *supercluster->roots_) {
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

void TextAutosizer::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
}

}  // namespace blink
