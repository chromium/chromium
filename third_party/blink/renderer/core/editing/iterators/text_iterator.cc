/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"

#include <unicode/utf16.h>

#include "build/build_config.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_legend_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_meter_element.h"
#include "third_party/blink/renderer/core/html/html_progress_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

template <typename Strategy>
TextIteratorBehavior AdjustBehaviorFlags(const TextIteratorBehavior&);

template <>
TextIteratorBehavior AdjustBehaviorFlags<EditingStrategy>(
    const TextIteratorBehavior& behavior) {
  if (!behavior.ForSelectionToString())
    return behavior;
  return TextIteratorBehavior::Builder(behavior)
      .SetExcludeAutofilledValue(true)
      .Build();
}

template <>
TextIteratorBehavior AdjustBehaviorFlags<EditingInFlatTreeStrategy>(
    const TextIteratorBehavior& behavior) {
  return TextIteratorBehavior::Builder(behavior)
      .SetExcludeAutofilledValue(behavior.ForSelectionToString() ||
                                 behavior.ExcludeAutofilledValue())
      .SetEntersOpenShadowRoots(false)
      .Build();
}

static inline bool HasDisplayContents(const Node& node) {
  auto* element = DynamicTo<Element>(node);
  return element && element->HasDisplayContentsStyle();
}

// Checks if |advance()| skips the descendants of |node|, which is the case if
// |node| is neither a shadow root nor the owner of a layout object.
static bool NotSkipping(const Node& node) {
  return node.GetLayoutObject() || HasDisplayContents(node) ||
         (IsA<ShadowRoot>(node) && node.OwnerShadowHost()->GetLayoutObject());
}

template <typename Strategy>
const Node* StartNode(const Node* start_container, unsigned start_offset) {
  if (start_container->IsCharacterDataNode())
    return start_container;
  if (Node* child = Strategy::ChildAt(*start_container, start_offset))
    return child;
  if (!start_offset)
    return start_container;
  return Strategy::NextSkippingChildren(*start_container);
}

template <typename Strategy>
const Node* EndNode(const Node& end_container, unsigned end_offset) {
  if (!end_container.IsCharacterDataNode() && end_offset)
    return Strategy::ChildAt(end_container, end_offset - 1);
  return nullptr;
}

// This function is like Range::PastLastNode, except for the fact that it can
// climb up out of shadow trees and ignores all nodes that will be skipped in
// |advance()|.
template <typename Strategy>
const Node* PastLastNode(const Node& range_end_container,
                         unsigned range_end_offset) {
  if (!range_end_container.IsCharacterDataNode() &&
      NotSkipping(range_end_container)) {
    for (Node* next = Strategy::ChildAt(range_end_container, range_end_offset);
         next; next = Strategy::NextSibling(*next)) {
      if (NotSkipping(*next))
        return next;
    }
  }
  for (const Node* node = &range_end_container; node;) {
    const Node* parent = ParentCrossingShadowBoundaries<Strategy>(*node);
    if (parent && NotSkipping(*parent)) {
      if (Node* next = Strategy::NextSibling(*node))
        return next;
    }
    node = parent;
  }
  return nullptr;
}

// Figure out the initial value of shadow_depth_: the depth of start_container's
// tree scope from the common ancestor tree scope.
template <typename Strategy>
unsigned ShadowDepthOf(const Node& start_container, const Node& end_container);

template <>
unsigned ShadowDepthOf<EditingStrategy>(const Node& start_container,
                                        const Node& end_container) {
  const TreeScope* common_ancestor_tree_scope =
      start_container.GetTreeScope().CommonAncestorTreeScope(
          end_container.GetTreeScope());
  DCHECK(common_ancestor_tree_scope);
  unsigned shadow_depth = 0;
  for (const TreeScope* tree_scope = &start_container.GetTreeScope();
       tree_scope != common_ancestor_tree_scope;
       tree_scope = tree_scope->ParentTreeScope())
    ++shadow_depth;
  return shadow_depth;
}

template <>
unsigned ShadowDepthOf<EditingInFlatTreeStrategy>(const Node& start_container,
                                                  const Node& end_container) {
  return 0;
}

bool IsRenderedAsTable(const Node* node) {
  if (!node || !node->IsElementNode())
    return false;
  LayoutObject* layout_object = node->GetLayoutObject();
  return layout_object && layout_object->IsTable();
}

bool ShouldHandleChildren(const Node& node,
                          const TextIteratorBehavior& behavior) {
  // To support |TextIteratorEmitsImageAltText|, we don't traversal child
  // nodes, in flat tree.
  if (IsA<HTMLImageElement>(node))
    return false;
  // Traverse internals of text control elements in flat tree only when
  // |EntersTextControls| flag is set.
  if (!behavior.EntersTextControls() && IsTextControl(node))
    return false;

  if (!behavior.IgnoresDisplayLock()) {
    if (auto* element = DynamicTo<Element>(node)) {
      if (auto* context = element->GetDisplayLockContext()) {
        return !context->IsLocked() ||
               context->IsActivatable(DisplayLockActivationReason::kSelection);
      }
    }
  }
  return true;
}

}  // namespace

template <typename Strategy>
TextIteratorAlgorithm<Strategy>::TextIteratorAlgorithm(
    const EphemeralRangeTemplate<Strategy>& range,
    const TextIteratorBehavior& behavior)
    : TextIteratorAlgorithm(range.StartPosition(),
                            range.EndPosition(),
                            behavior) {}

template <typename Strategy>
TextIteratorAlgorithm<Strategy>::TextIteratorAlgorithm(
    const PositionTemplate<Strategy>& start,
    const PositionTemplate<Strategy>& end,
    const TextIteratorBehavior& behavior)
    : start_container_(start.ComputeContainerNode()),
      start_offset_(start.ComputeOffsetInContainerNode()),
      end_container_(end.ComputeContainerNode()),
      end_offset_(end.ComputeOffsetInContainerNode()),
      end_node_(EndNode<Strategy>(*end_container_, end_offset_)),
      past_end_node_(PastLastNode<Strategy>(*end_container_, end_offset_)),
      node_(StartNode<Strategy>(start_container_, start_offset_)),
      iteration_progress_(kHandledNone),
      shadow_depth_(
          ShadowDepthOf<Strategy>(*start_container_, *end_container_)),
      behavior_(AdjustBehaviorFlags<Strategy>(behavior)),
      text_state_(behavior_),
      text_node_handler_(behavior_, &text_state_) {
  DCHECK(start_container_);
  DCHECK(end_container_);

  // TODO(dglazkov): TextIterator should not be created for documents that don't
  // have a frame, but it currently still happens in some cases. See
  // http://crbug.com/591877 for details.
  DCHECK(!start.GetDocument()->View() ||
         !start.GetDocument()->View()->NeedsLayout());
  DCHECK(!start.GetDocument()->NeedsLayoutTreeUpdate());
  // To avoid renderer hang, we use |CHECK_LE()| to catch the bad callers
  // in release build.
  CHECK_LE(start, end);

  if (!node_)
    return;

  fully_clipped_stack_.SetUpFullyClippedStack(node_);

  // Identify the first run.
  Advance();
}

template <typename Strategy>
TextIteratorAlgorithm<Strategy>::~TextIteratorAlgorithm() {
  if (!handle_shadow_root_)
    return;
  const Document& document = OwnerDocument();
  if (behavior_.ForSelectionToString())
    document.CountUse(WebFeature::kSelectionToStringWithShadowTree);
  if (behavior_.ForWindowFind())
    document.CountUse(WebFeature::kWindowFindWithShadowTree);
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::IsInsideAtomicInlineElement() const {
  if (AtEnd() || length() != 1 || !node_)
    return false;

  LayoutObject* layout_object = node_->GetLayoutObject();
  return layout_object && layout_object->IsAtomicInlineLevel();
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::HandleRememberedProgress() {
  // Handle remembered node that needed a newline after the text node's newline
  if (needs_another_newline_) {
    // Emit the extra newline, and position it *inside* node_, after node_'s
    // contents, in case it's a block, in the same way that we position the
    // first newline. The range for the emitted newline should start where the
    // line break begins.
    // FIXME: It would be cleaner if we emitted two newlines during the last
    // iteration, instead of using needs_another_newline_.
    Node* last_child = Strategy::LastChild(*node_);
    const Node* base_node = last_child ? last_child : node_;
    EmitChar16AfterNode('\n', *base_node);
    needs_another_newline_ = false;
    return true;
  }

  if (needs_handle_replaced_element_) {
    HandleReplacedElement();
    if (text_state_.PositionNode())
      return true;
  }

  // Try to emit more text runs if we are handling a text node.
  return text_node_handler_.HandleRemainingTextRuns();
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::Advance() {
  if (should_stop_)
    return;

  if (node_)
    DCHECK(!node_->GetDocument().NeedsLayoutTreeUpdate()) << node_;

  text_state_.ResetRunInformation();

  if (HandleRememberedProgress())
    return;

  while (node_ && (node_ != past_end_node_ || shadow_depth_)) {
    // TODO(crbug.com/1296290): Disable this DCHECK as it's troubling CrOS engs.
#if DCHECK_IS_ON() && !BUILDFLAG(IS_CHROMEOS)
    // |node_| shouldn't be after |past_end_node_|.
    if (past_end_node_) {
      DCHECK_LE(PositionTemplate<Strategy>(node_, 0),
                PositionTemplate<Strategy>(past_end_node_, 0));
    }
#endif

    if (!should_stop_ && StopsOnFormControls() &&
        HTMLFormControlElement::EnclosingFormControlElement(node_))
      should_stop_ = true;

    // if the range ends at offset 0 of an element, represent the
    // position, but not the content, of that element e.g. if the
    // node is a blockflow element, emit a newline that
    // precedes the element
    if (node_ == end_container_ && !end_offset_) {
      RepresentNodeOffsetZero();
      node_ = nullptr;
      return;
    }

    // If an element is locked, we shouldn't recurse down into its children
    // since they might not have up-to-date layout. In particular, they might
    // not have the NG offset mapping which is required. The display lock can
    // still be bypassed by marking the iterator behavior to ignore display
    // lock.
    const bool locked =
        !behavior_.IgnoresDisplayLock() &&
        DisplayLockUtilities::LockedInclusiveAncestorPreventingLayout(*node_);

    LayoutObject* layout_object = node_->GetLayoutObject();
    if (!layout_object || locked) {
      if (!locked && (IsA<ShadowRoot>(node_) || HasDisplayContents(*node_))) {
        // Shadow roots or display: contents elements don't have LayoutObjects,
        // but we want to visit children anyway.
        iteration_progress_ = iteration_progress_ < kHandledNode
                                  ? kHandledNode
                                  : iteration_progress_;
        handle_shadow_root_ = IsA<ShadowRoot>(node_);
      } else {
        iteration_progress_ = kHandledChildren;
      }
    } else {
      // Enter author shadow roots, from youngest, if any and if necessary.
      if (iteration_progress_ < kHandledOpenShadowRoots) {
        auto* element = DynamicTo<Element>(node_);
        if (std::is_same<Strategy, EditingStrategy>::value &&
            EntersOpenShadowRoots() && element && element->OpenShadowRoot()) {
          ShadowRoot* youngest_shadow_root = element->OpenShadowRoot();
          DCHECK(youngest_shadow_root->IsOpen());
          node_ = youngest_shadow_root;
          iteration_progress_ = kHandledNone;
          ++shadow_depth_;
          fully_clipped_stack_.PushFullyClippedState(node_);
          continue;
        }

        iteration_progress_ = kHandledOpenShadowRoots;
      }

      // Enter user-agent shadow root, if necessary.
      if (iteration_progress_ < kHandledUserAgentShadowRoot) {
        if (std::is_same<Strategy, EditingStrategy>::value &&
            EntersTextControls() && layout_object->IsTextControl()) {
          ShadowRoot* user_agent_shadow_root =
              To<Element>(node_)->UserAgentShadowRoot();
          DCHECK(user_agent_shadow_root->IsUserAgent());
          node_ = user_agent_shadow_root;
          iteration_progress_ = kHandledNone;
          ++shadow_depth_;
          fully_clipped_stack_.PushFullyClippedState(node_);
          continue;
        }
        iteration_progress_ = kHandledUserAgentShadowRoot;
      }

      // Handle the current node according to its type.
      if (iteration_progress_ < kHandledNode) {
        if (!SkipsUnselectableContent() || layout_object->IsSelectable()) {
          auto* html_element = DynamicTo<HTMLElement>(*node_);
          if (layout_object->IsText() &&
              node_->getNodeType() ==
                  Node::kTextNode) {  // FIXME: What about kCdataSectionNode?
            if (!fully_clipped_stack_.Top() || IgnoresStyleVisibility())
              HandleTextNode();
          } else if (layout_object &&
                     (layout_object->IsImage() ||
                      layout_object->IsLayoutEmbeddedContent() ||
                      (html_element &&
                       (IsA<HTMLFormControlElement>(html_element) ||
                        IsA<HTMLLegendElement>(html_element) ||
                        IsA<HTMLImageElement>(html_element) ||
                        IsA<HTMLMeterElement>(html_element) ||
                        IsA<HTMLProgressElement>(html_element))))) {
            HandleReplacedElement();
          } else {
            HandleNonTextNode();
          }
        }
        iteration_progress_ = kHandledNode;
        if (text_state_.PositionNode())
          return;
      }
    }

    // Find a new current node to handle in depth-first manner,
    // calling exitNode() as we come back thru a parent node.
    //
    // 1. Iterate over child nodes, if we haven't done yet.
    Node* next = iteration_progress_ < kHandledChildren &&
                         ShouldHandleChildren(*node_, behavior_)
                     ? Strategy::FirstChild(*node_)
                     : nullptr;
    if (!next) {
      // We are skipping children, check that |past_end_node_| is not a
      // descendant, since we shouldn't iterate past it.
      if (past_end_node_ && Strategy::IsDescendantOf(*past_end_node_, *node_)) {
        node_ = past_end_node_;
        iteration_progress_ = kHandledNone;
        fully_clipped_stack_.Pop();
        DCHECK(AtEnd());
        return;
      }

      // 2. If we've already iterated children or they are not available, go
      // to the next sibling node.
      next = Strategy::NextSibling(*node_);
      if (!next) {
        // 3. If we are at the last child, go up the node tree until we find a
        // next sibling.
        ContainerNode* parent_node = Strategy::Parent(*node_);
        while (!next && parent_node) {
          if (node_ == end_node_ ||
              Strategy::IsDescendantOf(*end_container_, *parent_node)) {
            return;
          }
          // We should call the ExitNode() always if |node_| has a layout
          // object or not and it's the last child under |parent_node|.
          bool have_layout_object = node_->GetLayoutObject();
          node_ = parent_node;
          fully_clipped_stack_.Pop();
          parent_node = Strategy::Parent(*node_);
          if (RuntimeEnabledFeatures::
                  CallExitNodeWithoutLayoutObjectEnabled() ||
              have_layout_object) {
            ExitNode();
          }
          if (text_state_.PositionNode()) {
            iteration_progress_ = kHandledChildren;
            return;
          }
          next = Strategy::NextSibling(*node_);
        }

        if (!next && !parent_node && shadow_depth_) {
          // 4. Reached the top of a shadow root. If it's created by author,
          // then try to visit the next
          // sibling shadow root, if any.
          const auto* shadow_root = DynamicTo<ShadowRoot>(node_);
          if (!shadow_root) {
            NOTREACHED_IN_MIGRATION();
            should_stop_ = true;
            return;
          }
          if (shadow_root->IsOpen()) {
            // We are the shadow root; exit from here and go back to
            // where we were.
            node_ = &shadow_root->host();
            iteration_progress_ = kHandledOpenShadowRoots;
            --shadow_depth_;
            fully_clipped_stack_.Pop();
          } else {
            // If we are in a closed or user-agent shadow root, then go back
            // to the host.
            // TODO(kochi): Make sure we treat closed shadow as user agent
            // shadow here.
            DCHECK(shadow_root->GetMode() == ShadowRootMode::kClosed ||
                   shadow_root->IsUserAgent());
            node_ = &shadow_root->host();
            iteration_progress_ = kHandledUserAgentShadowRoot;
            --shadow_depth_;
            fully_clipped_stack_.Pop();
          }
          continue;
        }
      }
      fully_clipped_stack_.Pop();
    }

    // set the new current node
    node_ = next;
    if (node_)
      fully_clipped_stack_.PushFullyClippedState(node_);
    iteration_progress_ = kHandledNone;

    // how would this ever be?
    if (text_state_.PositionNode())
      return;
  }
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::HandleTextNode() {
  if (ExcludesAutofilledValue()) {
    TextControlElement* control = EnclosingTextControl(node_);
    // For security reason, we don't expose suggested value if it is
    // auto-filled.
    // TODO(crbug.com/1472209): Only hide suggested value of previews.
    if (control && (control->IsAutofilled() || control->IsPreviewed())) {
      return;
    }
  }

  DCHECK_NE(last_text_node_, node_)
      << "We should never call HandleTextNode on the same node twice";
  const auto* text = To<Text>(node_);
  last_text_node_ = text;

  // TODO(editing-dev): Introduce a |DOMOffsetRange| class so that we can pass
  // an offset range with unbounded endpoint(s) in an easy but still clear way.
  if (node_ != start_container_) {
    if (node_ != end_container_)
      text_node_handler_.HandleTextNodeWhole(text);
    else
      text_node_handler_.HandleTextNodeEndAt(text, end_offset_);
    return;
  }
  if (node_ != end_container_) {
    text_node_handler_.HandleTextNodeStartFrom(text, start_offset_);
    return;
  }
  text_node_handler_.HandleTextNodeInRange(text, start_offset_, end_offset_);
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::SupportsAltText(const Node& node) {
  const auto* element = DynamicTo<HTMLElement>(node);
  if (!element)
    return false;

  // FIXME: Add isSVGImageElement.
  if (IsA<HTMLImageElement>(*element))
    return true;

  auto* html_input_element = DynamicTo<HTMLInputElement>(element);
  if (html_input_element &&
      html_input_element->FormControlType() == FormControlType::kInputImage) {
    return true;
  }
  return false;
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::HandleReplacedElement() {
  needs_handle_replaced_element_ = false;

  if (fully_clipped_stack_.Top())
    return;

  LayoutObject* layout_object = node_->GetLayoutObject();
  if (layout_object->Style()->UsedVisibility() != EVisibility::kVisible &&
      !IgnoresStyleVisibility()) {
    return;
  }

  if (EmitsObjectReplacementCharacter()) {
    EmitChar16AsNode(kObjectReplacementCharacter, *node_);
    return;
  }

  DCHECK_EQ(last_text_node_, text_node_handler_.GetNode());

  if (EntersTextControls() && layout_object->IsTextControl()) {
    // The shadow tree should be already visited.
    return;
  }

  if (EmitsCharactersBetweenAllVisiblePositions()) {
    // We want replaced elements to behave like punctuation for boundary
    // finding, and to simply take up space for the selection preservation
    // code in moveParagraphs, so we use a comma.
    EmitChar16AsNode(',', *node_);
    return;
  }

  if (EmitsImageAltText() && TextIterator::SupportsAltText(*node_)) {
    text_state_.EmitAltText(To<HTMLElement>(*node_));
    return;
  }
  // TODO(editing-dev): We can remove |UpdateForReplacedElement()| call when
  // we address web test failures (text diff by newlines only) and unit
  // tests, e.g. TextIteratorTest.IgnoreAltTextInTextControls.
  text_state_.UpdateForReplacedElement(*node_);
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::ShouldEmitTabBeforeNode(
    const Node& node) {
  LayoutObject* r = node.GetLayoutObject();

  // Table cells are delimited by tabs.
  if (!r || !IsTableCell(&node))
    return false;

  // Want a tab before every cell other than the first one
  const auto* rc = To<LayoutTableCell>(r);
  const LayoutTable* t = rc->Table();
  return t && !t->IsFirstCell(*rc);
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::ShouldEmitNewlineForNode(
    const Node& node,
    bool emits_original_text) {
  LayoutObject* layout_object = node.GetLayoutObject();

  if (layout_object ? !layout_object->IsBR() : !IsA<HTMLBRElement>(node))
    return false;
  return emits_original_text ||
         !(node.IsInShadowTree() &&
           IsA<HTMLInputElement>(*node.OwnerShadowHost()));
}

static bool ShouldEmitNewlinesBeforeAndAfterNode(const Node& node) {
  // Block flow (versus inline flow) is represented by having
  // a newline both before and after the element.
  LayoutObject* r = node.GetLayoutObject();
  if (!r) {
    if (HasDisplayContents(node))
      return false;
    return (node.HasTagName(html_names::kBlockquoteTag) ||
            node.HasTagName(html_names::kDdTag) ||
            node.HasTagName(html_names::kDivTag) ||
            node.HasTagName(html_names::kDlTag) ||
            node.HasTagName(html_names::kDtTag) ||
            node.HasTagName(html_names::kH1Tag) ||
            node.HasTagName(html_names::kH2Tag) ||
            node.HasTagName(html_names::kH3Tag) ||
            node.HasTagName(html_names::kH4Tag) ||
            node.HasTagName(html_names::kH5Tag) ||
            node.HasTagName(html_names::kH6Tag) ||
            node.HasTagName(html_names::kHrTag) ||
            node.HasTagName(html_names::kLiTag) ||
            node.HasTagName(html_names::kListingTag) ||
            node.HasTagName(html_names::kOlTag) ||
            node.HasTagName(html_names::kPTag) ||
            node.HasTagName(html_names::kPreTag) ||
            node.HasTagName(html_names::kTrTag) ||
            node.HasTagName(html_names::kUlTag));
  }

  // Need to make an exception for option and optgroup, because we want to
  // keep the legacy behavior before we added layoutObjects to them.
  if (IsA<HTMLOptionElement>(node) || IsA<HTMLOptGroupElement>(node))
    return false;

  // Need to make an exception for table cells, because they are blocks, but we
  // want them tab-delimited rather than having newlines before and after.
  if (IsTableCell(&node))
    return false;

  // Need to make an exception for table row elements, because they are neither
  // "inline" or "LayoutBlock", but we want newlines for them.
  if (r->IsTableRow()) {
    const LayoutTable* t = To<LayoutTableRow>(r)->Table();
    if (t && !t->IsInline()) {
      return true;
    }
  }

  return !r->IsInline() && r->IsLayoutBlock() &&
         !r->IsFloatingOrOutOfFlowPositioned() && !r->IsBody() &&
         !r->IsRubyText();
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::ShouldEmitNewlineAfterNode(
    const Node& node) {
  // FIXME: It should be better but slower to create a VisiblePosition here.
  if (!ShouldEmitNewlinesBeforeAndAfterNode(node))
    return false;
  // Check if this is the very last layoutObject in the document.
  // If so, then we should not emit a newline.
  const Node* next = &node;
  do {
    next = Strategy::NextSkippingChildren(*next);
    if (next && next->GetLayoutObject())
      return true;
  } while (next);
  return false;
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::ShouldEmitNewlineBeforeNode(
    const Node& node) {
  return ShouldEmitNewlinesBeforeAndAfterNode(node);
}

static bool ShouldEmitExtraNewlineForNode(const Node* node) {
  // https://html.spec.whatwg.org/C/#the-innertext-idl-attribute
  // Append two required linebreaks after a P element.
  LayoutObject* r = node->GetLayoutObject();
  if (!r || !r->IsBox())
    return false;

  return node->HasTagName(html_names::kPTag);
}

// Whether or not we should emit a character as we enter node_ (if it's a
// container) or as we hit it (if it's atomic).
template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::ShouldRepresentNodeOffsetZero() {
  if (EmitsCharactersBetweenAllVisiblePositions() && IsRenderedAsTable(node_))
    return true;

  // Leave element positioned flush with start of a paragraph
  // (e.g. do not insert tab before a table cell at the start of a paragraph)
  if (text_state_.LastCharacter() == '\n')
    return false;

  // Otherwise, show the position if we have emitted any characters
  if (text_state_.HasEmitted())
    return true;

  // We've not emitted anything yet. Generally, there is no need for any
  // positioning then. The only exception is when the element is visually not in
  // the same line as the start of the range (e.g. the range starts at the end
  // of the previous paragraph).
  // NOTE: Creating VisiblePositions and comparing them is relatively expensive,
  // so we make quicker checks to possibly avoid that. Another check that we
  // could make is is whether the inline vs block flow changed since the
  // previous visible element. I think we're already in a special enough case
  // that that won't be needed, tho.

  // No character needed if this is the first node in the range.
  if (node_ == start_container_)
    return false;

  // If we are outside the start container's subtree, assume we need to emit.
  // FIXME: start_container_ could be an inline block
  if (!Strategy::IsDescendantOf(*node_, *start_container_))
    return true;

  // If we started as start_container_ offset 0 and the current node is a
  // descendant of the start container, we already had enough context to
  // correctly decide whether to emit after a preceding block. We chose not to
  // emit (has_emitted_ is false), so don't second guess that now.
  // NOTE: Is this really correct when node_ is not a leftmost descendant?
  // Probably immaterial since we likely would have already emitted something by
  // now.
  if (!start_offset_)
    return false;

  // If this node is unrendered or invisible the VisiblePosition checks below
  // won't have much meaning.
  // Additionally, if the range we are iterating over contains huge sections of
  // unrendered content, we would create VisiblePositions on every call to this
  // function without this check.
  if (!node_->GetLayoutObject() ||
      node_->GetLayoutObject()->Style()->UsedVisibility() !=
          EVisibility::kVisible ||
      (node_->GetLayoutObject()->IsLayoutBlockFlow() &&
       !To<LayoutBlock>(node_->GetLayoutObject())->Size().height &&
       !IsA<HTMLBodyElement>(*node_))) {
    return false;
  }

  // The startPos.isNotNull() check is needed because the start could be before
  // the body, and in that case we'll get null. We don't want to put in newlines
  // at the start in that case.
  // The currPos.isNotNull() check is needed because positions in non-HTML
  // content (like SVG) do not have visible positions, and we don't want to emit
  // for them either.
  const VisiblePositionTemplate<Strategy> start_pos = CreateVisiblePosition(
      PositionTemplate<Strategy>(start_container_, start_offset_));
  const VisiblePositionTemplate<Strategy> curr_pos =
      VisiblePositionTemplate<Strategy>::BeforeNode(*node_);
  return start_pos.IsNotNull() && curr_pos.IsNotNull() &&
         !InSameLine(start_pos, curr_pos);
}

template <typename Strategy>
bool TextIteratorAlgorithm<Strategy>::ShouldEmitSpaceBeforeAndAfterNode(
    const Node& node) {
  return IsRenderedAsTable(&node) &&
         (node.GetLayoutObject()->IsInline() ||
          EmitsCharactersBetweenAllVisiblePositions());
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::RepresentNodeOffsetZero() {
  // Emit a character to show the positioning of node_.

  // TODO(editing-dev): We should rewrite this below code fragment to utilize
  // early-return style.
  // When we haven't been emitting any characters,
  // ShouldRepresentNodeOffsetZero() can create VisiblePositions, which is
  // expensive. So, we perform the inexpensive checks on |node_| to see if it
  // necessitates emitting a character first and will early return before
  // encountering ShouldRepresentNodeOffsetZero()s worse case behavior.
  if (ShouldEmitTabBeforeNode(*node_)) {
    if (ShouldRepresentNodeOffsetZero())
      EmitChar16BeforeNode('\t', *node_);
  } else if (ShouldEmitNewlineBeforeNode(*node_)) {
    if (ShouldRepresentNodeOffsetZero())
      EmitChar16BeforeNode('\n', *node_);
  } else if (ShouldEmitSpaceBeforeAndAfterNode(*node_)) {
    if (ShouldRepresentNodeOffsetZero())
      EmitChar16BeforeNode(kSpaceCharacter, *node_);
  }
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::HandleNonTextNode() {
  if (ShouldEmitNewlineForNode(*node_, EmitsOriginalText()))
    EmitChar16AsNode('\n', *node_);
  else if (EmitsCharactersBetweenAllVisiblePositions() &&
           node_->GetLayoutObject() && node_->GetLayoutObject()->IsHR())
    EmitChar16AsNode(kSpaceCharacter, *node_);
  else
    RepresentNodeOffsetZero();
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::ExitNode() {
  // prevent emitting a newline when exiting a collapsed block at beginning of
  // the range
  // FIXME: !has_emitted_ does not necessarily mean there was a collapsed
  // block... it could have been an hr (e.g.). Also, a collapsed block could
  // have height (e.g. a table) and therefore look like a blank line.
  if (!text_state_.HasEmitted())
    return;

  // Emit with a position *inside* node_, after node_'s contents, in
  // case it is a block, because the run should start where the
  // emitted character is positioned visually.
  Node* last_child = Strategy::LastChild(*node_);
  const Node* base_node = last_child ? last_child : node_;
  // FIXME: This shouldn't require the last_text_node to be true, but we can't
  // change that without making the logic in _web_attributedStringFromRange
  // match. We'll get that for free when we switch to use TextIterator in
  // _web_attributedStringFromRange. See <rdar://problem/5428427> for an example
  // of how this mismatch will cause problems.
  if (last_text_node_ && ShouldEmitNewlineAfterNode(*node_)) {
    // use extra newline to represent margin bottom, as needed
    const bool add_newline = !behavior_.SuppressesExtraNewlineEmission() &&
                             ShouldEmitExtraNewlineForNode(node_);

    // FIXME: We need to emit a '\n' as we leave an empty block(s) that
    // contain a VisiblePosition when doing selection preservation.
    if (text_state_.LastCharacter() != '\n') {
      // insert a newline with a position following this block's contents.
      EmitChar16AfterNode(kNewlineCharacter, *base_node);
      // remember whether to later add a newline for the current node
      DCHECK(!needs_another_newline_);
      needs_another_newline_ = add_newline;
    } else if (add_newline) {
      // insert a newline with a position following this block's contents.
      EmitChar16AfterNode(kNewlineCharacter, *base_node);
    }
  }

  // If nothing was emitted, see if we need to emit a space.
  if (!text_state_.PositionNode() && ShouldEmitSpaceBeforeAndAfterNode(*node_))
    EmitChar16AfterNode(kSpaceCharacter, *base_node);
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::EmitChar16AfterNode(UChar code_unit,
                                                          const Node& node) {
  text_state_.EmitChar16AfterNode(code_unit, node);
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::EmitChar16AsNode(UChar code_unit,
                                                       const Node& node) {
  text_state_.EmitChar16AsNode(code_unit, node);
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::EmitChar16BeforeNode(UChar code_unit,
                                                           const Node& node) {
  text_state_.EmitChar16BeforeNode(code_unit, node);
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy> TextIteratorAlgorithm<Strategy>::Range()
    const {
  // use the current run information, if we have it
  if (text_state_.PositionNode()) {
    return EphemeralRangeTemplate<Strategy>(StartPositionInCurrentContainer(),
                                            EndPositionInCurrentContainer());
  }

  // otherwise, return the end of the overall range we were given
  return EphemeralRangeTemplate<Strategy>(
      PositionTemplate<Strategy>(end_container_, end_offset_));
}

template <typename Strategy>
const Document& TextIteratorAlgorithm<Strategy>::OwnerDocument() const {
  return end_container_->GetDocument();
}

template <typename Strategy>
const Node* TextIteratorAlgorithm<Strategy>::GetNode() const {
  const Node& node = CurrentContainer();
  if (node.IsCharacterDataNode())
    return &node;
  return Strategy::ChildAt(node, StartOffsetInCurrentContainer());
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::StartOffsetInCurrentContainer() const {
  if (!text_state_.PositionNode())
    return end_offset_;
  EnsurePositionContainer();
  return text_state_.PositionStartOffset();
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::EndOffsetInCurrentContainer() const {
  if (!text_state_.PositionNode())
    return end_offset_;
  EnsurePositionContainer();
  return text_state_.PositionEndOffset();
}

template <typename Strategy>
const Node& TextIteratorAlgorithm<Strategy>::CurrentContainer() const {
  if (!text_state_.PositionNode())
    return *end_container_;
  EnsurePositionContainer();
  return *text_state_.PositionContainerNode();
}

template <typename Strategy>
void TextIteratorAlgorithm<Strategy>::EnsurePositionContainer() const {
  DCHECK(text_state_.PositionNode());
  if (text_state_.PositionContainerNode())
    return;
  const Node& node = *text_state_.PositionNode();
  const ContainerNode* parent = Strategy::Parent(node);
  DCHECK(parent);
  text_state_.UpdatePositionOffsets(*parent, Strategy::Index(node));
}

template <typename Strategy>
PositionTemplate<Strategy> TextIteratorAlgorithm<Strategy>::GetPositionBefore(
    int char16_offset) const {
  if (AtEnd()) {
    DCHECK_EQ(char16_offset, 0);
    return PositionTemplate<Strategy>(CurrentContainer(),
                                      StartOffsetInCurrentContainer());
  }
  DCHECK_GE(char16_offset, 0);
  DCHECK_LT(char16_offset, length());
  DCHECK_GE(length(), 1);
  const Node& node = *text_state_.PositionNode();
  if (text_state_.IsInTextNode() || text_state_.IsBeforeCharacter()) {
    return PositionTemplate<Strategy>(
        node, text_state_.PositionStartOffset() + char16_offset);
  }
  if (auto* text_node = DynamicTo<Text>(node)) {
    if (text_state_.IsAfterPositionNode())
      return PositionTemplate<Strategy>(node, text_node->length());
    return PositionTemplate<Strategy>(node, 0);
  }
  if (text_state_.IsAfterPositionNode())
    return PositionTemplate<Strategy>::AfterNode(node);
  DCHECK(!text_state_.IsBeforeChildren());
  return PositionTemplate<Strategy>::BeforeNode(node);
}

template <typename Strategy>
PositionTemplate<Strategy> TextIteratorAlgorithm<Strategy>::GetPositionAfter(
    int char16_offset) const {
  if (AtEnd()) {
    DCHECK_EQ(char16_offset, 0);
    return PositionTemplate<Strategy>(CurrentContainer(),
                                      EndOffsetInCurrentContainer());
  }
  DCHECK_GE(char16_offset, 0);
  DCHECK_LT(char16_offset, length());
  DCHECK_GE(length(), 1);
  const Node& node = *text_state_.PositionNode();
  if (text_state_.IsBeforeCharacter()) {
    return PositionTemplate<Strategy>(
        node, text_state_.PositionStartOffset() + char16_offset);
  }
  if (text_state_.IsInTextNode()) {
    return PositionTemplate<Strategy>(
        node, text_state_.PositionStartOffset() + char16_offset + 1);
  }
  if (auto* text_node = DynamicTo<Text>(node)) {
    if (text_state_.IsBeforePositionNode())
      return PositionTemplate<Strategy>(node, 0);
    return PositionTemplate<Strategy>(node, text_node->length());
  }
  if (text_state_.IsBeforePositionNode())
    return PositionTemplate<Strategy>::BeforeNode(node);
  DCHECK(!text_state_.IsBeforeChildren());
  return PositionTemplate<Strategy>::AfterNode(node);
}

template <typename Strategy>
PositionTemplate<Strategy>
TextIteratorAlgorithm<Strategy>::StartPositionInCurrentContainer() const {
  return PositionTemplate<Strategy>::EditingPositionOf(
      &CurrentContainer(), StartOffsetInCurrentContainer());
}

template <typename Strategy>
PositionTemplate<Strategy>
TextIteratorAlgorithm<Strategy>::EndPositionInCurrentContainer() const {
  return PositionTemplate<Strategy>::EditingPositionOf(
      &CurrentContainer(), EndOffsetInCurrentContainer());
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::RangeLength(
    const PositionTemplate<Strategy>& start,
    const PositionTemplate<Strategy>& end,
    const TextIteratorBehavior& behavior) {
  DCHECK(start.GetDocument());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      start.GetDocument()->Lifecycle());

  int length = 0;
  for (TextIteratorAlgorithm<Strategy> it(start, end, behavior); !it.AtEnd();
       it.Advance())
    length += it.length();

  return length;
}

template <typename Strategy>
int TextIteratorAlgorithm<Strategy>::RangeLength(
    const EphemeralRangeTemplate<Strategy>& range,
    const TextIteratorBehavior& behavior) {
  return RangeLength(range.StartPosition(), range.EndPosition(), behavior);
}

// --------

template <typename Strategy>
static String CreatePlainText(const EphemeralRangeTemplate<Strategy>& range,
                              const TextIteratorBehavior& behavior) {
  if (range.IsNull())
    return g_empty_string;

  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      range.StartPosition().GetDocument()->Lifecycle());

  TextIteratorAlgorithm<Strategy> it(range.StartPosition(), range.EndPosition(),
                                     behavior);

  if (it.AtEnd())
    return g_empty_string;

  // The initial buffer size can be critical for performance:
  // https://bugs.webkit.org/show_bug.cgi?id=81192
  static const unsigned kInitialCapacity = 1 << 15;

  StringBuilder builder;
  builder.ReserveCapacity(kInitialCapacity);

  for (; !it.AtEnd(); it.Advance())
    it.GetTextState().AppendTextToStringBuilder(builder);

  if (builder.empty())
    return g_empty_string;

  return builder.ToString();
}

String PlainText(const EphemeralRange& range,
                 const TextIteratorBehavior& behavior) {
  return CreatePlainText<EditingStrategy>(range, behavior);
}

String PlainText(const EphemeralRangeInFlatTree& range,
                 const TextIteratorBehavior& behavior) {
  return CreatePlainText<EditingInFlatTreeStrategy>(range, behavior);
}

template class CORE_TEMPLATE_EXPORT TextIteratorAlgorithm<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    TextIteratorAlgorithm<EditingInFlatTreeStrategy>;

}  // namespace blink
