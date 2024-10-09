/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/editing/frame_selection.h"

#include <stdio.h>

#include <optional>

#include "base/auto_reset.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/accessibility/blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/accessibility/scoped_blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/node_with_index.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/caret_display_item_client.h"
#include "third_party/blink/renderer/core/editing/commands/typing_command.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_caret.h"
#include "third_party/blink/renderer/core/editing/granularity_strategy.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/layout_selection.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#include "third_party/blink/renderer/core/editing/selection_editor.h"
#include "third_party/blink/renderer/core/editing/selection_modifier.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/unicode_utilities.h"
#include "ui/gfx/geometry/quad_f.h"

#define EDIT_DEBUG 0

namespace blink {

static inline bool ShouldAlwaysUseDirectionalSelection(LocalFrame* frame) {
  return frame->GetEditor().Behavior().ShouldConsiderSelectionAsDirectional();
}

FrameSelection::FrameSelection(LocalFrame& frame)
    : frame_(frame),
      layout_selection_(MakeGarbageCollected<LayoutSelection>(*this)),
      selection_editor_(MakeGarbageCollected<SelectionEditor>(frame)),
      granularity_(TextGranularity::kCharacter),
      x_pos_for_vertical_arrow_navigation_(NoXPosForVerticalArrowNavigation()),
      focused_(frame.GetPage() &&
               frame.GetPage()->GetFocusController().FocusedFrame() == frame),
      is_directional_(ShouldAlwaysUseDirectionalSelection(frame_)),
      frame_caret_(
          MakeGarbageCollected<FrameCaret>(frame, *selection_editor_)) {}

FrameSelection::~FrameSelection() = default;

const EffectPaintPropertyNode& FrameSelection::CaretEffectNode() const {
  return frame_caret_->CaretEffectNode();
}

bool FrameSelection::IsAvailable() const {
  return SynchronousMutationObserver::GetDocument();
}

Document& FrameSelection::GetDocument() const {
  DCHECK(IsAvailable());
  return *SynchronousMutationObserver::GetDocument();
}

VisibleSelection FrameSelection::ComputeVisibleSelectionInDOMTree() const {
  return selection_editor_->ComputeVisibleSelectionInDOMTree();
}

VisibleSelectionInFlatTree FrameSelection::ComputeVisibleSelectionInFlatTree()
    const {
  return selection_editor_->ComputeVisibleSelectionInFlatTree();
}

const SelectionInDOMTree& FrameSelection::GetSelectionInDOMTree() const {
  return selection_editor_->GetSelectionInDOMTree();
}

Element* FrameSelection::RootEditableElementOrDocumentElement() const {
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);

  Element* selection_root =
      ComputeVisibleSelectionInDOMTree().RootEditableElement();
  // Note that RootEditableElementOrDocumentElement can return null if the
  // documentElement is null.
  return selection_root ? selection_root : GetDocument().documentElement();
}

wtf_size_t FrameSelection::CharacterIndexForPoint(
    const gfx::Point& point) const {
  const EphemeralRange range = GetFrame()->GetEditor().RangeForPoint(point);
  if (range.IsNull())
    return kNotFound;
  Element* const editable = RootEditableElementOrDocumentElement();
  if (!editable) {
    return kNotFound;
  }
  PlainTextRange plain_text_range = PlainTextRange::Create(*editable, range);
  if (plain_text_range.IsNull())
    return kNotFound;
  return plain_text_range.Start();
}

VisibleSelection FrameSelection::ComputeVisibleSelectionInDOMTreeDeprecated()
    const {
  // TODO(editing-dev): Hoist UpdateStyleAndLayout
  // to caller. See http://crbug.com/590369 for more details.
  Position anchor = GetSelectionInDOMTree().Anchor();
  Position focus = GetSelectionInDOMTree().Focus();
  std::optional<DisplayLockUtilities::ScopedForcedUpdate> force_locks;
  if (anchor != focus && anchor.ComputeContainerNode() &&
      focus.ComputeContainerNode()) {
    force_locks = DisplayLockUtilities::ScopedForcedUpdate(
        MakeGarbageCollected<Range>(GetDocument(), anchor, focus),
        DisplayLockContext::ForcedPhase::kLayout);
  } else {
    force_locks = DisplayLockUtilities::ScopedForcedUpdate(
        anchor.AnchorNode(), DisplayLockContext::ForcedPhase::kLayout);
  }
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  return ComputeVisibleSelectionInDOMTree();
}

void FrameSelection::MoveCaretSelection(const gfx::Point& point) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());

  Element* const editable =
      ComputeVisibleSelectionInDOMTree().RootEditableElement();
  if (!editable)
    return;

  const VisiblePosition position = CreateVisiblePosition(
      PositionForContentsPointRespectingEditingBoundary(point, GetFrame()));
  SelectionInDOMTree::Builder builder;
  if (position.IsNotNull())
    builder.Collapse(position.ToPositionWithAffinity());
  SetSelection(builder.Build(), SetSelectionOptions::Builder()
                                    .SetShouldCloseTyping(true)
                                    .SetShouldClearTypingStyle(true)
                                    .SetSetSelectionBy(SetSelectionBy::kUser)
                                    .SetShouldShowHandle(true)
                                    .SetIsDirectional(IsDirectional())
                                    .Build());
}

void FrameSelection::SetSelection(const SelectionInDOMTree& selection,
                                  const SetSelectionOptions& data) {
  if (SetSelectionDeprecated(selection, data))
    DidSetSelectionDeprecated(selection, data);
}

void FrameSelection::SetSelectionAndEndTyping(
    const SelectionInDOMTree& selection) {
  SetSelection(selection, SetSelectionOptions::Builder()
                              .SetShouldCloseTyping(true)
                              .SetShouldClearTypingStyle(true)
                              .Build());
}

static void AssertUserSelection(const SelectionInDOMTree& selection,
                                const SetSelectionOptions& options) {
// User's selection start/end should have same editability.
#if DCHECK_IS_ON()
  if (!options.ShouldShowHandle() &&
      options.GetSetSelectionBy() != SetSelectionBy::kUser)
    return;
  Node* anchor_editable_root = RootEditableElementOf(selection.Anchor());
  Node* focus_editable_root = RootEditableElementOf(selection.Focus());
  DCHECK_EQ(anchor_editable_root, focus_editable_root) << selection;
#endif
}

bool FrameSelection::SetSelectionDeprecated(
    const SelectionInDOMTree& new_selection,
    const SetSelectionOptions& passed_options) {
  SetSelectionOptions::Builder options_builder(passed_options);
  if (ShouldAlwaysUseDirectionalSelection(frame_)) {
    options_builder.SetIsDirectional(true);
  }
  const SetSelectionOptions options = options_builder.Build();

  if (granularity_strategy_ && !options.DoNotClearStrategy())
    granularity_strategy_->Clear();
  granularity_ = options.Granularity();

  // TODO(yosin): We should move to call |TypingCommand::closeTyping()| to
  // |Editor| class.
  if (options.ShouldCloseTyping())
    TypingCommand::CloseTyping(frame_);

  if (options.ShouldClearTypingStyle())
    frame_->GetEditor().ClearTypingStyle();

  const SelectionInDOMTree old_selection_in_dom_tree =
      selection_editor_->GetSelectionInDOMTree();
  const bool is_changed = old_selection_in_dom_tree != new_selection;
  const bool should_show_handle = options.ShouldShowHandle();
  if (!is_changed && is_handle_visible_ == should_show_handle &&
      is_directional_ == options.IsDirectional())
    return false;
  Document& current_document = GetDocument();
  if (is_changed) {
    AssertUserSelection(new_selection, options);
    selection_editor_->SetSelectionAndEndTyping(new_selection);
    NotifyDisplayLockForSelectionChange(
        current_document, old_selection_in_dom_tree, new_selection);
  }
  is_directional_ = options.IsDirectional();
  should_shrink_next_tap_ = options.ShouldShrinkNextTap();
  is_handle_visible_ = should_show_handle;
  ScheduleVisualUpdateForVisualOverflowIfNeeded();

  frame_->GetEditor().RespondToChangedSelection();
  DCHECK_EQ(current_document, GetDocument());
  return true;
}

void FrameSelection::DidSetSelectionDeprecated(
    const SelectionInDOMTree& new_selection,
    const SetSelectionOptions& options) {
  Document& current_document = GetDocument();
  const SetSelectionBy set_selection_by = options.GetSetSelectionBy();

  // Provides details to accessibility about the selection change throughout the
  // current call stack.
  //
  // If the selection is currently being modified via the "Modify" method, we
  // should already have more detailed information on the stack than can be
  // deduced in this method.
  std::optional<ScopedBlinkAXEventIntent> scoped_blink_ax_event_intent;
  if (current_document.ExistingAXObjectCache()) {
    scoped_blink_ax_event_intent.emplace(
        is_being_modified_ ? BlinkAXEventIntent()
        : new_selection.IsNone()
            ? BlinkAXEventIntent::FromClearedSelection(set_selection_by)
            : BlinkAXEventIntent::FromNewSelection(
                  options.Granularity(), new_selection.IsAnchorFirst(),
                  set_selection_by),
        &current_document);
  }

  if (!new_selection.IsNone() && !options.DoNotSetFocus()) {
    SetFocusedNodeIfNeeded();
    // |setFocusedNodeIfNeeded()| dispatches sync events "FocusOut" and
    // "FocusIn", |frame_| may associate to another document.
    if (!IsAvailable() || GetDocument() != current_document) {
      // editing/selection/move-selection-detached-frame-crash.html reaches
      // here. See http://crbug.com/1015710.
      return;
    }
  }

  frame_caret_->StopCaretBlinkTimer();
  UpdateAppearance();

  // Always clear the x position used for vertical arrow navigation.
  // It will be restored by the vertical arrow navigation code if necessary.
  x_pos_for_vertical_arrow_navigation_ = NoXPosForVerticalArrowNavigation();

  // TODO(yosin): Can we move this to at end of this function?
  // This may dispatch a synchronous focus-related events.
  if (!options.DoNotSetFocus()) {
    SelectFrameElementInParentIfFullySelected();
    if (!IsAvailable() || GetDocument() != current_document) {
      // editing/selection/selectallchildren-crash.html and
      // editing/selection/longpress-selection-in-iframe-removed-crash.html
      // reach here.
      return;
    }
  }

  NotifyTextControlOfSelectionChange(set_selection_by);
  if (set_selection_by == SetSelectionBy::kUser) {
    const CursorAlignOnScroll align = options.GetCursorAlignOnScroll();
    mojom::blink::ScrollAlignment alignment;

    if (frame_->GetEditor()
            .Behavior()
            .ShouldCenterAlignWhenSelectionIsRevealed()) {
      alignment = (align == CursorAlignOnScroll::kAlways)
                      ? ScrollAlignment::CenterAlways()
                      : ScrollAlignment::CenterIfNeeded();
    } else {
      alignment = (align == CursorAlignOnScroll::kAlways)
                      ? ScrollAlignment::TopAlways()
                      : ScrollAlignment::ToEdgeIfNeeded();
    }

    RevealSelection(alignment, kRevealExtent);
  }

  NotifyAccessibilityForSelectionChange();
  NotifyCompositorForSelectionChange();
  NotifyEventHandlerForSelectionChange();

  // Dispatch selectionchange events per element based on the new spec:
  // https://w3c.github.io/selection-api/#selectionchange-event
  if (RuntimeEnabledFeatures::DispatchSelectionchangeEventPerElementEnabled()) {
    TextControlElement* text_control =
        EnclosingTextControl(GetSelectionInDOMTree().Anchor());
    if (text_control && !text_control->IsInShadowTree()) {
      text_control->ScheduleSelectionchangeEvent();
    } else {
      GetDocument().ScheduleSelectionchangeEvent();
    }
  }
  // When DispatchSelectionchangeEventPerElement is disabled, fall back to old
  // path.
  else {
    // The task source should be kDOMManipulation, but the spec doesn't say
    // anything about this.
    frame_->DomWindow()->EnqueueDocumentEvent(
        *Event::Create(event_type_names::kSelectionchange),
        TaskType::kMiscPlatformAPI);
  }
}

void FrameSelection::SetSelectionForAccessibility(
    const SelectionInDOMTree& selection,
    const SetSelectionOptions& options) {
  ClearDocumentCachedRange();

  const bool did_set = SetSelectionDeprecated(selection, options);
  CacheRangeOfDocument(CreateRange(selection.ComputeRange()));
  if (did_set)
    DidSetSelectionDeprecated(selection, options);
}

void FrameSelection::NodeChildrenWillBeRemoved(ContainerNode& container) {
  if (!container.InActiveDocument())
    return;
  // TODO(yosin): We should move to call |TypingCommand::CloseTypingIfNeeded()|
  // to |Editor| class.
  TypingCommand::CloseTypingIfNeeded(frame_);
}

void FrameSelection::NodeWillBeRemoved(Node& node) {
  // There can't be a selection inside a fragment, so if a fragment's node is
  // being removed, the selection in the document that created the fragment
  // needs no adjustment.
  if (!node.InActiveDocument())
    return;
  // TODO(yosin): We should move to call |TypingCommand::CloseTypingIfNeeded()|
  // to |Editor| class.
  TypingCommand::CloseTypingIfNeeded(frame_);
}

void FrameSelection::DidChangeFocus() {
  UpdateAppearance();
}

static DispatchEventResult DispatchSelectStart(
    const VisibleSelection& selection) {
  Node* select_start_target = selection.Focus().ComputeContainerNode();
  if (!select_start_target)
    return DispatchEventResult::kNotCanceled;

  return select_start_target->DispatchEvent(
      *Event::CreateCancelableBubble(event_type_names::kSelectstart));
}

// The return value of |FrameSelection::modify()| is different based on
// value of |userTriggered| parameter.
// When |userTriggered| is |userTriggered|, |modify()| returns false if
// "selectstart" event is dispatched and canceled, otherwise returns true.
// When |userTriggered| is |NotUserTrigged|, return value specifies whether
// selection is modified or not.
bool FrameSelection::Modify(SelectionModifyAlteration alter,
                            SelectionModifyDirection direction,
                            TextGranularity granularity,
                            SetSelectionBy set_selection_by) {
  SelectionModifier selection_modifier(*GetFrame(), GetSelectionInDOMTree(),
                                       x_pos_for_vertical_arrow_navigation_);
  selection_modifier.SetSelectionIsDirectional(IsDirectional());
  const bool modified =
      selection_modifier.Modify(alter, direction, granularity);
  if (set_selection_by == SetSelectionBy::kUser &&
      selection_modifier.Selection().IsRange() &&
      ComputeVisibleSelectionInDOMTree().IsCaret() &&
      DispatchSelectStart(ComputeVisibleSelectionInDOMTree()) !=
          DispatchEventResult::kNotCanceled) {
    return false;
  }

  // |DispatchSelectStart()| can change document hosted by |frame_|.
  if (!IsAvailable()) {
    return false;
  }

  if (!modified) {
    if (set_selection_by == SetSelectionBy::kSystem)
      return false;
    // If spatial navigation enabled, focus navigator will move focus to
    // another element. See snav-input.html and snav-textarea.html
    if (IsSpatialNavigationEnabled(frame_))
      return false;
    // Even if selection isn't changed, we prevent to default action, e.g.
    // scroll window when caret is at end of content editable.
    return true;
  }

  // Provides details to accessibility about the selection change throughout the
  // current call stack.
  base::AutoReset<bool> is_being_modified_resetter(&is_being_modified_, true);
  const PlatformWordBehavior platform_word_behavior =
      frame_->GetEditor().Behavior().ShouldSkipSpaceWhenMovingRight()
          ? PlatformWordBehavior::kWordSkipSpaces
          : PlatformWordBehavior::kWordDontSkipSpaces;
  Document& document = GetDocument();
  std::optional<ScopedBlinkAXEventIntent> scoped_blink_ax_event_intent;
  if (document.ExistingAXObjectCache()) {
    scoped_blink_ax_event_intent.emplace(
        BlinkAXEventIntent::FromModifiedSelection(
            alter, direction, granularity, set_selection_by,
            selection_modifier.DirectionOfSelection(), platform_word_behavior),
        &document);
  }

  // For MacOS only selection is directionless at the beginning.
  // Selection gets direction on extent.
  const bool selection_is_directional =
      alter == SelectionModifyAlteration::kExtend ||
      ShouldAlwaysUseDirectionalSelection(frame_);

  SetSelection(selection_modifier.Selection().AsSelection(),
               SetSelectionOptions::Builder()
                   .SetShouldCloseTyping(true)
                   .SetShouldClearTypingStyle(true)
                   .SetSetSelectionBy(set_selection_by)
                   .SetIsDirectional(selection_is_directional)
                   .Build());

  if (granularity == TextGranularity::kLine ||
      granularity == TextGranularity::kParagraph)
    x_pos_for_vertical_arrow_navigation_ =
        selection_modifier.XPosForVerticalArrowNavigation();

  if (set_selection_by == SetSelectionBy::kUser)
    granularity_ = TextGranularity::kCharacter;

  ScheduleVisualUpdateForVisualOverflowIfNeeded();

  return true;
}

void FrameSelection::Clear() {
  granularity_ = TextGranularity::kCharacter;
  if (granularity_strategy_)
    granularity_strategy_->Clear();
  SetSelectionAndEndTyping(SelectionInDOMTree());
  is_handle_visible_ = false;
  is_directional_ = ShouldAlwaysUseDirectionalSelection(frame_);
}

bool FrameSelection::SelectionHasFocus() const {
  // TODO(editing-dev): Hoist UpdateStyleAndLayout
  // to caller. See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  if (ComputeVisibleSelectionInFlatTree().IsNone())
    return false;
  const Node* current =
      ComputeVisibleSelectionInFlatTree().Start().ComputeContainerNode();
  if (!current)
    return false;

  // No focused element means document root has focus.
  Element* const focused_element = GetDocument().FocusedElement()
                                       ? GetDocument().FocusedElement()
                                       : GetDocument().documentElement();
  if (!focused_element || focused_element->IsScrollControlPseudoElement()) {
    return false;
  }

  if (focused_element->IsTextControl())
    return focused_element->ContainsIncludingHostElements(*current);

  // Selection has focus if it contains the focused element.
  const PositionInFlatTree& focused_position =
      PositionInFlatTree::FirstPositionInNode(*focused_element);
  if (ComputeVisibleSelectionInFlatTree().Start() <= focused_position &&
      ComputeVisibleSelectionInFlatTree().End() >= focused_position)
    return true;

  bool is_editable = IsEditable(*current);
  do {
    // If the selection is within an editable sub tree and that sub tree
    // doesn't have focus, the selection doesn't have focus either.
    if (is_editable && !IsEditable(*current))
      return false;

    // Selection has focus if its sub tree has focus.
    if (current == focused_element)
      return true;
    current = current->ParentOrShadowHostNode();
  } while (current);

  return false;
}

bool FrameSelection::IsHidden() const {
  if (SelectionHasFocus())
    return false;

  const Node* start =
      ComputeVisibleSelectionInDOMTree().Start().ComputeContainerNode();
  if (!start)
    return true;

  // The selection doesn't have focus, so hide everything but range selections.
  if (!GetSelectionInDOMTree().IsRange())
    return true;

  // Here we know we have an unfocused range selection. Let's say that
  // selection resides inside a text control. Since the selection doesn't have
  // focus neither does the text control. Meaning, if the selection indeed
  // resides inside a text control, it should be hidden.
  return EnclosingTextControl(start);
}

void FrameSelection::DidAttachDocument(Document* document) {
  DCHECK(document);
  selection_editor_->DidAttachDocument(document);
  SetDocument(document);
}

void FrameSelection::ContextDestroyed() {
  granularity_ = TextGranularity::kCharacter;

  layout_selection_->ContextDestroyed();

  frame_->GetEditor().ClearTypingStyle();
}

void FrameSelection::LayoutBlockWillBeDestroyed(const LayoutBlock& block) {
  frame_caret_->LayoutBlockWillBeDestroyed(block);
}

void FrameSelection::UpdateStyleAndLayoutIfNeeded() {
  frame_caret_->UpdateStyleAndLayoutIfNeeded();
}

void FrameSelection::InvalidatePaint(const LayoutBlock& block,
                                     const PaintInvalidatorContext& context) {
  frame_caret_->InvalidatePaint(block, context);
}

void FrameSelection::EnsureInvalidationOfPreviousLayoutBlock() {
  frame_caret_->EnsureInvalidationOfPreviousLayoutBlock();
}

bool FrameSelection::ShouldPaintCaret(const LayoutBlock& block) const {
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);
  bool result = frame_caret_->ShouldPaintCaret(block);
  DCHECK(!result ||
         (ComputeVisibleSelectionInDOMTree().IsCaret() &&
          (IsEditablePosition(ComputeVisibleSelectionInDOMTree().Start()) ||
           frame_->IsCaretBrowsingEnabled())));
  return result;
}

bool FrameSelection::ShouldPaintCaret(
    const PhysicalBoxFragment& box_fragment) const {
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kLayoutClean);
  bool result = frame_caret_->ShouldPaintCaret(box_fragment);
  DCHECK(!result ||
         (ComputeVisibleSelectionInDOMTree().IsCaret() &&
          (IsEditablePosition(ComputeVisibleSelectionInDOMTree().Start()) ||
           frame_->IsCaretBrowsingEnabled())));
  return result;
}

gfx::Rect FrameSelection::AbsoluteCaretBounds() const {
  DCHECK(ComputeVisibleSelectionInDOMTree().IsValidFor(*frame_->GetDocument()));
  return frame_caret_->AbsoluteCaretBounds();
}

bool FrameSelection::ComputeAbsoluteBounds(gfx::Rect& anchor,
                                           gfx::Rect& focus) const {
  if (!IsAvailable() || GetSelectionInDOMTree().IsNone())
    return false;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame_->GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kSelection);
  if (ComputeVisibleSelectionInDOMTree().IsNone()) {
    // plugins/mouse-capture-inside-shadow.html reaches here.
    return false;
  }

  return selection_editor_->ComputeAbsoluteBounds(anchor, focus);
}

void FrameSelection::PaintCaret(GraphicsContext& context,
                                const PhysicalOffset& paint_offset) {
  frame_caret_->PaintCaret(context, paint_offset);
}

bool FrameSelection::Contains(const PhysicalOffset& point) {
  if (!GetDocument().GetLayoutView())
    return false;

  // This is a workaround of the issue that we sometimes get null from
  // ComputeVisibleSelectionInDOMTree(), but non-null from flat tree.
  // By running this, in case we get null, we also set the cached result in flat
  // tree into null, so that this function can return false correctly.
  // See crbug.com/846527 for details.
  // TODO(editing-dev): Fix the inconsistency and then remove this call.
  ComputeVisibleSelectionInDOMTree();

  // Treat a collapsed selection like no selection.
  const VisibleSelectionInFlatTree& visible_selection =
      ComputeVisibleSelectionInFlatTree();
  if (!visible_selection.IsRange())
    return false;

  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location(point);
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  const PositionInFlatTreeWithAffinity pos_with_affinity =
      FromPositionInDOMTree<EditingInFlatTreeStrategy>(result.GetPosition());
  if (pos_with_affinity.IsNull())
    return false;

  const VisiblePositionInFlatTree& visible_start =
      visible_selection.VisibleStart();
  const VisiblePositionInFlatTree& visible_end = visible_selection.VisibleEnd();
  if (visible_start.IsNull() || visible_end.IsNull())
    return false;

  const PositionInFlatTree& start = visible_start.DeepEquivalent();
  const PositionInFlatTree& end = visible_end.DeepEquivalent();
  const PositionInFlatTree& pos = pos_with_affinity.GetPosition();
  return start.CompareTo(pos) <= 0 && pos.CompareTo(end) <= 0;
}

// Workaround for the fact that it's hard to delete a frame.
// Call this after doing user-triggered selections to make it easy to delete the
// frame you entirely selected. Can't do this implicitly as part of every
// setSelection call because in some contexts it might not be good for the focus
// to move to another frame. So instead we call it from places where we are
// selecting with the mouse or the keyboard after setting the selection.
void FrameSelection::SelectFrameElementInParentIfFullySelected() {
  // Find the parent frame; if there is none, then we have nothing to do.
  Frame* parent = frame_->Tree().Parent();
  if (!parent)
    return;
  Page* page = frame_->GetPage();
  if (!page)
    return;

  // Check if the selection contains the entire frame contents; if not, then
  // there is nothing to do.
  if (!GetSelectionInDOMTree().IsRange())
    return;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);

  if (!IsStartOfDocument(ComputeVisibleSelectionInDOMTree().VisibleStart()))
    return;
  if (!IsEndOfDocument(ComputeVisibleSelectionInDOMTree().VisibleEnd()))
    return;

  // FIXME: This is not yet implemented for cross-process frame relationships.
  auto* parent_local_frame = DynamicTo<LocalFrame>(parent);
  if (!parent_local_frame)
    return;

  // Get to the <iframe> or <frame> (or even <object>) element in the parent
  // frame.
  // FIXME: Doesn't work for OOPI.
  HTMLFrameOwnerElement* owner_element = frame_->DeprecatedLocalOwner();
  if (!owner_element)
    return;
  ContainerNode* owner_element_parent = owner_element->parentNode();
  if (!owner_element_parent)
    return;

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited. See http://crbug.com/590369 for more details.
  owner_element_parent->GetDocument().UpdateStyleAndLayout(
      DocumentUpdateReason::kSelection);

  // This method's purpose is it to make it easier to select iframes (in order
  // to delete them).  Don't do anything if the iframe isn't deletable.
  if (!blink::IsEditable(*owner_element_parent))
    return;

  // Focus on the parent frame, and then select from before this element to
  // after.
  page->GetFocusController().SetFocusedFrame(parent);
  // SetFocusedFrame can dispatch synchronous focus/blur events.  The document
  // tree might be modified.
  if (!owner_element->isConnected() ||
      owner_element->GetDocument() != parent_local_frame->GetDocument())
    return;
  parent_local_frame->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position::BeforeNode(*owner_element),
                            Position::AfterNode(*owner_element))
          .Build(),
      SetSelectionOptions());
}

// Returns a shadow tree node for legacy shadow trees, a child of the
// ShadowRoot node for new shadow trees, or 0 for non-shadow trees.
static Node* NonBoundaryShadowTreeRootNode(const Position& position) {
  return position.AnchorNode() && !position.AnchorNode()->IsShadowRoot()
             ? position.AnchorNode()->NonBoundaryShadowTreeRootNode()
             : nullptr;
}

void FrameSelection::SelectAll(SetSelectionBy set_selection_by,
                               bool canonicalize_selection) {
  if (auto* select_element =
          DynamicTo<HTMLSelectElement>(GetDocument().FocusedElement())) {
    if (select_element->CanSelectAll()) {
      select_element->SelectAll();
      return;
    }
  }

  Node* root = nullptr;
  Node* select_start_target = nullptr;
  if (set_selection_by == SetSelectionBy::kUser && IsHidden()) {
    // Hidden selection appears as no selection to user, in which case user-
    // triggered SelectAll should act as if there is no selection.
    root = GetDocument().documentElement();
    select_start_target = GetDocument().body();
  } else if (ComputeVisibleSelectionInDOMTree().IsContentEditable()) {
    root = HighestEditableRoot(ComputeVisibleSelectionInDOMTree().Start());
    if (Node* shadow_root = NonBoundaryShadowTreeRootNode(
            ComputeVisibleSelectionInDOMTree().Start()))
      select_start_target = shadow_root->OwnerShadowHost();
    else
      select_start_target = root;
  } else {
    root = NonBoundaryShadowTreeRootNode(
        ComputeVisibleSelectionInDOMTree().Start());
    if (root) {
      select_start_target = root->OwnerShadowHost();
    } else {
      root = GetDocument().documentElement();
      select_start_target = GetDocument().body();
    }
  }
  if (!root || EditingIgnoresContent(*root))
    return;

  if (select_start_target) {
    const Document& expected_document = GetDocument();
    if (select_start_target->DispatchEvent(
            *Event::CreateCancelableBubble(event_type_names::kSelectstart)) !=
        DispatchEventResult::kNotCanceled)
      return;
    // The frame may be detached due to selectstart event.
    if (!IsAvailable()) {
      // Reached by editing/selection/selectstart_detach_frame.html
      return;
    }
    // |root| may be detached due to selectstart event.
    if (!root->isConnected() || expected_document != root->GetDocument())
      return;
  }

  const SelectionInDOMTree& dom_selection =
      SelectionInDOMTree::Builder().SelectAllChildren(*root).Build();
  if (canonicalize_selection) {
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  }
  SetSelection(canonicalize_selection
                   ? CreateVisibleSelection(dom_selection).AsSelection()
                   : dom_selection,
               SetSelectionOptions::Builder()
                   .SetShouldCloseTyping(true)
                   .SetShouldClearTypingStyle(true)
                   .SetShouldShowHandle(IsHandleVisible())
                   .Build());

  SelectFrameElementInParentIfFullySelected();
  // TODO(editing-dev): Should we pass in set_selection_by?
  NotifyTextControlOfSelectionChange(SetSelectionBy::kUser);
  if (IsHandleVisible()) {
    ContextMenuAllowedScope scope;
    frame_->GetEventHandler().ShowNonLocatedContextMenu(nullptr,
                                                        kMenuSourceTouch);
  }
}

void FrameSelection::SelectAll() {
  SelectAll(SetSelectionBy::kSystem, false);
}

// Implementation of |SVGTextControlElement::selectSubString()|
void FrameSelection::SelectSubString(const Element& element,
                                     int offset,
                                     int length) {
  // Find selection start
  VisiblePosition start = VisiblePosition::FirstPositionInNode(element);
  for (int i = 0; i < offset; ++i)
    start = NextPositionOf(start);
  if (start.IsNull())
    return;

  // Find selection end
  VisiblePosition end(start);
  for (int i = 0; i < length; ++i)
    end = NextPositionOf(end);
  if (end.IsNull())
    return;

  // TODO(editing-dev): We assume |start| and |end| are not null and we don't
  // known when |start| and |end| are null. Once we get a such case, we check
  // null for |start| and |end|.
  SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(start.DeepEquivalent(), end.DeepEquivalent())
          .SetAffinity(start.Affinity())
          .Build());
}

void FrameSelection::NotifyAccessibilityForSelectionChange() {
  AXObjectCache* cache = GetDocument().ExistingAXObjectCache();
  if (!cache)
    return;
  Node* anchor = GetSelectionInDOMTree().Focus().ComputeContainerNode();
  if (anchor) {
    cache->SelectionChanged(anchor);
  } else {
    cache->SelectionChanged(RootEditableElementOrDocumentElement());
  }
}

void FrameSelection::NotifyCompositorForSelectionChange() {
  if (!RuntimeEnabledFeatures::CompositedSelectionUpdateEnabled())
    return;

  ScheduleVisualUpdate();
}

void FrameSelection::NotifyEventHandlerForSelectionChange() {
  frame_->GetEventHandler().GetSelectionController().NotifySelectionChanged();
}

void FrameSelection::NotifyDisplayLockForSelectionChange(
    Document& document,
    const SelectionInDOMTree& old_selection,
    const SelectionInDOMTree& new_selection) {
  if (DisplayLockUtilities::NeedsSelectionChangedUpdate(document) ||
      (!old_selection.IsNone() && old_selection.GetDocument() != document &&
       DisplayLockUtilities::NeedsSelectionChangedUpdate(
           *old_selection.GetDocument()))) {
    // The old selection might not be valid, and thus not iterable. If
    // that's the case, notify that all selection was removed and use an empty
    // range as the old selection.
    EphemeralRangeInFlatTree old_range;
    if (old_selection.IsValidFor(document)) {
      old_range = ToEphemeralRangeInFlatTree(old_selection.ComputeRange());
    } else {
      DisplayLockUtilities::SelectionRemovedFromDocument(document);
    }
    DisplayLockUtilities::SelectionChanged(
        old_range, ToEphemeralRangeInFlatTree(new_selection.ComputeRange()));
  }
}

void FrameSelection::FocusedOrActiveStateChanged() {
  bool active_and_focused = FrameIsFocusedAndActive();

  // Trigger style invalidation from the focused element. Even though
  // the focused element hasn't changed, the evaluation of focus pseudo
  // selectors are dependent on whether the frame is focused and active.
  if (Element* element = GetDocument().FocusedElement()) {
    element->FocusStateChanged();
  }

  // Selection style may depend on the active state of the document, so style
  // and paint must be invalidated when active status changes.
  if (GetDocument().GetLayoutView()) {
    layout_selection_->InvalidateStyleAndPaintForSelection();
  }
  GetDocument().UpdateStyleAndLayoutTree();

  // Caret appears in the active frame.
  if (active_and_focused) {
    SetSelectionFromNone();
  }
  frame_caret_->SetCaretEnabled(active_and_focused);

  // Update for caps lock state
  frame_->GetEventHandler().CapsLockStateMayHaveChanged();
}

void FrameSelection::PageActivationChanged() {
  FocusedOrActiveStateChanged();
}

void FrameSelection::SetFrameIsFocused(bool flag) {
  if (focused_ == flag)
    return;
  focused_ = flag;

  FocusedOrActiveStateChanged();
}

bool FrameSelection::FrameIsFocusedAndActive() const {
  return focused_ && frame_->GetPage() &&
         frame_->GetPage()->GetFocusController().IsActive();
}

void FrameSelection::CommitAppearanceIfNeeded() {
  return layout_selection_->Commit();
}

void FrameSelection::DidLayout() {
  UpdateAppearance();
}

void FrameSelection::UpdateAppearance() {
  DCHECK(frame_->ContentLayoutObject());
  frame_caret_->ScheduleVisualUpdateForPaintInvalidationIfNeeded();
  layout_selection_->SetHasPendingSelection();
}

void FrameSelection::NotifyTextControlOfSelectionChange(
    SetSelectionBy set_selection_by) {
  TextControlElement* text_control =
      EnclosingTextControl(GetSelectionInDOMTree().Anchor());
  if (!text_control)
    return;
  text_control->SelectionChanged(set_selection_by == SetSelectionBy::kUser);
}

// Helper function that tells whether a particular node is an element that has
// an entire LocalFrame and LocalFrameView, a <frame>, <iframe>, or <object>.
static bool IsFrameElement(const Node* n) {
  if (!n)
    return false;
  if (auto* embedded = DynamicTo<LayoutEmbeddedContent>(n->GetLayoutObject()))
    return embedded->ChildFrameView();
  return false;
}

void FrameSelection::SetFocusedNodeIfNeeded() {
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);

  if (ComputeVisibleSelectionInDOMTree().IsNone() || !FrameIsFocused()) {
    return;
  }

  if (Element* target =
          ComputeVisibleSelectionInDOMTree().RootEditableElement()) {
    // Walk up the DOM tree to search for a node to focus.
    GetDocument().UpdateStyleAndLayoutTree();
    while (target) {
      // We don't want to set focus on a subframe when selecting in a parent
      // frame, so add the !isFrameElement check here. There's probably a better
      // way to make this work in the long term, but this is the safest fix at
      // this time.
      if (target->IsFocusable() && !IsFrameElement(target)) {
        frame_->GetPage()->GetFocusController().SetFocusedElement(target,
                                                                  frame_);
        return;
      }
      target = target->ParentOrShadowHostElement();
    }
    GetDocument().ClearFocusedElement();
  }
}

static EphemeralRangeInFlatTree ComputeRangeForSerialization(
    const SelectionInDOMTree& selection_in_dom_tree) {
  const SelectionInFlatTree& selection =
      ConvertToSelectionInFlatTree(selection_in_dom_tree);
  // TODO(crbug.com/1019152): Once we know the root cause of having
  // seleciton with |Anchor().IsNull() != Focus().IsNull()|, we should get rid
  // of this if-statement.
  if (selection.Anchor().IsNull() || selection.Focus().IsNull()) {
    DCHECK(selection.IsNone());
    return EphemeralRangeInFlatTree();
  }
  const EphemeralRangeInFlatTree& range = selection.ComputeRange();
  const PositionInFlatTree& start =
      CreateVisiblePosition(range.StartPosition()).DeepEquivalent();
  const PositionInFlatTree& end =
      CreateVisiblePosition(range.EndPosition()).DeepEquivalent();
  if (start.IsNull() || end.IsNull() || start >= end)
    return EphemeralRangeInFlatTree();
  return NormalizeRange(EphemeralRangeInFlatTree(start, end));
}

static String ExtractSelectedText(const FrameSelection& selection,
                                  TextIteratorBehavior behavior) {
  const EphemeralRangeInFlatTree& range =
      ComputeRangeForSerialization(selection.GetSelectionInDOMTree());
  // We remove '\0' characters because they are not visibly rendered to the
  // user.
  return PlainText(range, behavior).Replace(0, "");
}

String FrameSelection::SelectedHTMLForClipboard() const {
  const EphemeralRangeInFlatTree& range =
      ComputeRangeForSerialization(GetSelectionInDOMTree());
  return CreateMarkup(range.StartPosition(), range.EndPosition(),
                      CreateMarkupOptions::Builder()
                          .SetShouldAnnotateForInterchange(true)
                          .SetShouldResolveURLs(kResolveNonLocalURLs)
                          .SetIgnoresCSSTextTransformsForRenderedText(true)
                          .Build());
}

String FrameSelection::SelectedText(
    const TextIteratorBehavior& behavior) const {
  return ExtractSelectedText(*this, behavior);
}

String FrameSelection::SelectedText() const {
  return SelectedText(TextIteratorBehavior());
}

String FrameSelection::SelectedTextForClipboard() const {
  return ExtractSelectedText(
      *this, TextIteratorBehavior::Builder()
                 .SetEmitsImageAltText(
                     frame_->GetSettings() &&
                     frame_->GetSettings()->GetSelectionIncludesAltImageText())
                 .SetSkipsUnselectableContent(true)
                 .SetEntersTextControls(true)
                 .SetIgnoresCSSTextTransforms(true)
                 .Build());
}

PhysicalRect FrameSelection::AbsoluteUnclippedBounds() const {
  LocalFrameView* view = frame_->View();
  LayoutView* layout_view = frame_->ContentLayoutObject();

  if (!view || !layout_view)
    return PhysicalRect();

  return PhysicalRect(layout_selection_->AbsoluteSelectionBounds());
}

gfx::Rect FrameSelection::ComputeRectToScroll(
    RevealExtentOption reveal_extent_option) {
  const VisibleSelection& selection = ComputeVisibleSelectionInDOMTree();
  if (selection.IsCaret())
    return AbsoluteCaretBounds();
  DCHECK(selection.IsRange());
  if (reveal_extent_option == kRevealExtent) {
    return AbsoluteCaretBoundsOf(
        CreateVisiblePosition(selection.Focus()).ToPositionWithAffinity());
  }
  layout_selection_->SetHasPendingSelection();
  return layout_selection_->AbsoluteSelectionBounds();
}

// TODO(editing-dev): This should be done in FlatTree world.
void FrameSelection::RevealSelection(
    const mojom::blink::ScrollAlignment& alignment,
    RevealExtentOption reveal_extent_option) {
  DCHECK(IsAvailable());

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // Calculation of absolute caret bounds requires clean layout.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);

  const VisibleSelection& selection = ComputeVisibleSelectionInDOMTree();
  if (selection.IsNone())
    return;

  // FIXME: This code only handles scrolling the startContainer's layer, but
  // the selection rect could intersect more than just that.
  if (DocumentLoader* document_loader = frame_->Loader().GetDocumentLoader())
    document_loader->GetInitialScrollState().was_scrolled_by_user = true;
  const Position& start = selection.Start();
  DCHECK(start.AnchorNode());
  if (!start.AnchorNode()->GetLayoutObject()) {
    return;
  }

  // This function is needed to make sure that ComputeRectToScroll below has the
  // sticky offset info available before the computation.
  GetDocument().EnsurePaintLocationDataValidForNode(
      start.AnchorNode(), DocumentUpdateReason::kSelection);
  PhysicalRect selection_rect(ComputeRectToScroll(reveal_extent_option));
  if (selection_rect == PhysicalRect()) {
    return;
  }

  scroll_into_view_util::ScrollRectToVisible(
      *start.AnchorNode()->GetLayoutObject(), selection_rect,
      scroll_into_view_util::CreateScrollIntoViewParams(alignment, alignment));
  UpdateAppearance();
}

void FrameSelection::SetSelectionFromNone() {
  // Put a caret inside the body if the entire frame is editable (either the
  // entire WebView is editable or designMode is on for this document).

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);

  Document* document = frame_->GetDocument();
  if (!ComputeVisibleSelectionInDOMTree().IsNone() ||
      !(blink::IsEditable(*document))) {
    return;
  }

  Element* document_element = document->documentElement();
  if (!document_element)
    return;
  if (HTMLBodyElement* body =
          Traversal<HTMLBodyElement>::FirstChild(*document_element)) {
    SetSelection(SelectionInDOMTree::Builder()
                     .Collapse(FirstPositionInOrBeforeNode(*body))
                     .Build(),
                 SetSelectionOptions());
  }
}

#if DCHECK_IS_ON()

void FrameSelection::ShowTreeForThis() const {
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kSelection);

  ComputeVisibleSelectionInDOMTree().ShowTreeForThis();
}

#endif

void FrameSelection::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(layout_selection_);
  visitor->Trace(selection_editor_);
  visitor->Trace(frame_caret_);
  SynchronousMutationObserver::Trace(visitor);
}

void FrameSelection::ScheduleVisualUpdate() const {
  if (Page* page = frame_->GetPage())
    page->Animator().ScheduleVisualUpdate(&frame_->LocalFrameRoot());
}

void FrameSelection::ScheduleVisualUpdateForVisualOverflowIfNeeded() const {
  if (LocalFrameView* frame_view = frame_->View())
    frame_view->ScheduleVisualUpdateForVisualOverflowIfNeeded();
}

bool FrameSelection::SelectWordAroundCaret() {
  return SelectAroundCaret(TextGranularity::kWord,
                           HandleVisibility::kNotVisible,
                           ContextMenuVisibility::kNotVisible);
}

bool FrameSelection::SelectAroundCaret(
    TextGranularity text_granularity,
    HandleVisibility handle_visibility,
    ContextMenuVisibility context_menu_visibility) {
  CHECK(text_granularity == TextGranularity::kWord ||
        text_granularity == TextGranularity::kSentence)
      << "Only word and sentence granularities are supported for now";

  EphemeralRange selection_range =
      GetSelectionRangeAroundCaret(text_granularity);
  if (selection_range.IsNull()) {
    return false;
  }

  SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(selection_range.StartPosition())
          .Extend(selection_range.EndPosition())
          .Build(),
      SetSelectionOptions::Builder()
          .SetShouldCloseTyping(true)
          .SetShouldClearTypingStyle(true)
          .SetGranularity(text_granularity)
          .SetShouldShowHandle(handle_visibility == HandleVisibility::kVisible)
          .Build());

  if (context_menu_visibility == ContextMenuVisibility::kVisible) {
    ContextMenuAllowedScope scope;
    frame_->GetEventHandler().ShowNonLocatedContextMenu(
        /*override_target_element=*/nullptr, kMenuSourceTouch);
  }

  return true;
}

EphemeralRange FrameSelection::GetWordSelectionRangeAroundCaret() const {
  return GetSelectionRangeAroundCaret(TextGranularity::kWord);
}

EphemeralRange FrameSelection::GetSelectionRangeAroundCaretForTesting(
    TextGranularity text_granularity) const {
  return GetSelectionRangeAroundCaret(text_granularity);
}

GranularityStrategy* FrameSelection::GetGranularityStrategy() {
  // We do lazy initialization for granularity_strategy_, because if we
  // initialize it right in the constructor - the correct settings may not be
  // set yet.
  SelectionStrategy strategy_type = SelectionStrategy::kCharacter;
  Settings* settings = frame_ ? frame_->GetSettings() : nullptr;
  if (settings &&
      settings->GetSelectionStrategy() == SelectionStrategy::kDirection)
    strategy_type = SelectionStrategy::kDirection;

  if (granularity_strategy_ &&
      granularity_strategy_->GetType() == strategy_type)
    return granularity_strategy_.get();

  if (strategy_type == SelectionStrategy::kDirection)
    granularity_strategy_ = std::make_unique<DirectionGranularityStrategy>();
  else
    granularity_strategy_ = std::make_unique<CharacterGranularityStrategy>();
  return granularity_strategy_.get();
}

void FrameSelection::MoveRangeSelectionExtent(
    const gfx::Point& contents_point) {
  if (ComputeVisibleSelectionInDOMTree().IsNone())
    return;

  SetSelection(
      SelectionInDOMTree::Builder(
          GetGranularityStrategy()->UpdateExtent(contents_point, frame_))
          .Build(),
      SetSelectionOptions::Builder()
          .SetShouldCloseTyping(true)
          .SetShouldClearTypingStyle(true)
          .SetDoNotClearStrategy(true)
          .SetSetSelectionBy(SetSelectionBy::kUser)
          .SetShouldShowHandle(true)
          .Build());
}

void FrameSelection::MoveRangeSelection(const gfx::Point& base_point,
                                        const gfx::Point& extent_point,
                                        TextGranularity granularity) {
  const VisiblePosition& base_position =
      CreateVisiblePosition(PositionForContentsPointRespectingEditingBoundary(
          base_point, GetFrame()));
  const VisiblePosition& extent_position =
      CreateVisiblePosition(PositionForContentsPointRespectingEditingBoundary(
          extent_point, GetFrame()));
  MoveRangeSelectionInternal(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtentDeprecated(base_position.DeepEquivalent(),
                                      extent_position.DeepEquivalent())
          .SetAffinity(base_position.Affinity())
          .Build(),
      granularity);
}

void FrameSelection::MoveRangeSelectionInternal(
    const SelectionInDOMTree& new_selection,
    TextGranularity granularity) {
  if (new_selection.IsNone())
    return;

  const SelectionInDOMTree& selection =
      ExpandWithGranularity(new_selection, granularity);
  if (selection.IsNone())
    return;

  SetSelection(selection, SetSelectionOptions::Builder()
                              .SetShouldCloseTyping(true)
                              .SetShouldClearTypingStyle(true)
                              .SetGranularity(granularity)
                              .SetShouldShowHandle(IsHandleVisible())
                              .Build());
}

void FrameSelection::SetCaretEnabled(bool enabled) {
  frame_caret_->SetCaretEnabled(enabled);
}

void FrameSelection::SetCaretBlinkingSuspended(bool suspended) {
  frame_caret_->SetCaretBlinkingSuspended(suspended);
}

bool FrameSelection::IsCaretBlinkingSuspended() const {
  return frame_caret_->IsCaretBlinkingSuspended();
}

void FrameSelection::CacheRangeOfDocument(Range* range) {
  selection_editor_->CacheRangeOfDocument(range);
}

Range* FrameSelection::DocumentCachedRange() const {
  return selection_editor_->DocumentCachedRange();
}

void FrameSelection::ClearDocumentCachedRange() {
  selection_editor_->ClearDocumentCachedRange();
}

LayoutSelectionStatus FrameSelection::ComputeLayoutSelectionStatus(
    const InlineCursor& cursor) const {
  return layout_selection_->ComputeSelectionStatus(cursor);
}

SelectionState FrameSelection::ComputePaintingSelectionStateForCursor(
    const InlineCursorPosition& position) const {
  return layout_selection_->ComputePaintingSelectionStateForCursor(position);
}

bool FrameSelection::IsDirectional() const {
  return is_directional_;
}

void FrameSelection::MarkCacheDirty() {
  selection_editor_->MarkCacheDirty();
}

EphemeralRange FrameSelection::GetSelectionRangeAroundCaret(
    TextGranularity text_granularity) const {
  DCHECK(text_granularity == TextGranularity::kWord ||
         text_granularity == TextGranularity::kSentence)
      << "Only word and sentence granularities are supported for now";

  const VisibleSelection& selection = ComputeVisibleSelectionInDOMTree();
  // TODO(editing-dev): The use of VisibleSelection needs to be audited. See
  // http://crbug.com/657237 for more details.
  if (!selection.IsCaret()) {
    return EphemeralRange();
  }

  // Determine the selection range at each side of the caret, then prefer to set
  // a range that does not start with a separator character.
  const EphemeralRange next_range = GetSelectionRangeAroundPosition(
      text_granularity, selection.Start(), kNextWordIfOnBoundary);
  const String next_text = PlainText(next_range);
  if (!next_text.empty() && !IsSeparator(next_text.CharacterStartingAt(0))) {
    return next_range;
  }

  const EphemeralRange previous_range = GetSelectionRangeAroundPosition(
      text_granularity, selection.Start(), kPreviousWordIfOnBoundary);
  const String previous_text = PlainText(previous_range);
  if (!previous_text.empty() &&
      !IsSeparator(previous_text.CharacterStartingAt(0))) {
    return previous_range;
  }

  // Otherwise, select a range if it contains a non-separator character.
  if (!ContainsOnlySeparatorsOrEmpty(next_text)) {
    return next_range;
  } else if (!ContainsOnlySeparatorsOrEmpty(previous_text)) {
    return previous_range;
  }

  // Otherwise, don't select anything.
  return EphemeralRange();
}

EphemeralRange FrameSelection::GetSelectionRangeAroundPosition(
    TextGranularity text_granularity,
    Position position,
    WordSide word_side) const {
  Position start;
  Position end;
  // Use word granularity by default unless sentence granularity is explicitly
  // requested.
  if (text_granularity == TextGranularity::kSentence) {
    start = StartOfSentencePosition(position);
    end = EndOfSentence(position, SentenceTrailingSpaceBehavior::kOmitSpace)
              .GetPosition();
  } else {
    start = StartOfWordPosition(position, word_side);
    end = EndOfWordPosition(position, word_side);
  }

  // TODO(editing-dev): |StartOfWord()| and |EndOfWord()| should not make null
  // for non-null parameter. See http://crbug.com/872443.
  if (start.IsNull() || end.IsNull()) {
    return EphemeralRange();
  }

  if (start > end) {
    // Since word boundaries are computed on flat tree, they can be reversed
    // when mapped back to DOM.
    std::swap(start, end);
  }

  return EphemeralRange(start, end);
}

}  // namespace blink

#if DCHECK_IS_ON()

void ShowTree(const blink::FrameSelection& sel) {
  sel.ShowTreeForThis();
}

void ShowTree(const blink::FrameSelection* sel) {
  if (sel)
    sel->ShowTreeForThis();
  else
    LOG(INFO) << "Cannot showTree for <null> FrameSelection.";
}

#endif
