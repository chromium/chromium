/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FRAME_SELECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FRAME_SELECTION_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/synchronous_mutation_observer.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CaretDisplayItemClient;
class Element;
class LayoutBlock;
class LayoutText;
class LocalFrame;
class FrameCaret;
class GranularityStrategy;
class GraphicsContext;
class NGInlineCursor;
class Range;
class SelectionEditor;
class LayoutSelection;
enum class SelectionModifyAlteration;
enum class SelectionModifyDirection;
enum class SelectionState;
class TextIteratorBehavior;
struct PaintInvalidatorContext;
struct PhysicalOffset;
struct PhysicalRect;

enum RevealExtentOption { kRevealExtent, kDoNotRevealExtent };

enum class CaretVisibility;

enum class HandleVisibility { kNotVisible, kVisible };
enum class SelectSoftLineBreak { kNotSelected, kSelected };

// This is return type of ComputeLayoutSelectionStatus(cursor).
// This structure represents how the fragment is selected.
// |start|, |end| : Selection start/end offset. This offset is based on
//   the text of NGInlineNode of a parent block thus
//   |fragemnt.StartOffset <= start <= end <= fragment.EndOffset|.
// |start| == |end| means this fragment is not selected.
// |line_break| : This value represents If this fragment is selected and
// selection wraps soft line break.
struct LayoutSelectionStatus {
  STACK_ALLOCATED();

 public:
  LayoutSelectionStatus(unsigned passed_start,
                        unsigned passed_end,
                        SelectSoftLineBreak passed_line_break)
      : start(passed_start), end(passed_end), line_break(passed_line_break) {
    DCHECK_LE(start, end);
  }

  bool HasValidRange() const { return start < end; }

  bool operator==(const LayoutSelectionStatus& other) const {
    return start == other.start && end == other.end &&
           line_break == other.line_break;
  }

  unsigned start;
  unsigned end;
  SelectSoftLineBreak line_break;
};

enum class SelectionIncludeEnd { kInclude, kNotInclude };

struct LayoutTextSelectionStatus {
  STACK_ALLOCATED();

 public:
  LayoutTextSelectionStatus(unsigned passed_start,
                            unsigned passed_end,
                            SelectionIncludeEnd passed_include_end)
      : start(passed_start), end(passed_end), include_end(passed_include_end) {
    DCHECK_LE(start, end);
  }
  bool operator==(const LayoutTextSelectionStatus& other) const {
    return start == other.start && end == other.end &&
           include_end == other.include_end;
  }
  bool IsEmpty() const { return start == 0 && end == 0; }

  unsigned start;
  unsigned end;
  SelectionIncludeEnd include_end;
};

class CORE_EXPORT FrameSelection final
    : public GarbageCollected<FrameSelection>,
      public SynchronousMutationObserver {
 public:
  explicit FrameSelection(LocalFrame&);
  ~FrameSelection();

  bool IsAvailable() const;
  // You should not call |document()| when |!isAvailable()|.
  Document& GetDocument() const;
  LocalFrame* GetFrame() const { return frame_; }
  Element* RootEditableElementOrDocumentElement() const;
  size_t CharacterIndexForPoint(const IntPoint&) const;

  // An implementation of |WebFrame::moveCaretSelection()|
  void MoveCaretSelection(const IntPoint&);

  VisibleSelection ComputeVisibleSelectionInDOMTree() const;
  VisibleSelectionInFlatTree ComputeVisibleSelectionInFlatTree() const;

  // TODO(editing-dev): We should replace
  // |computeVisibleSelectionInDOMTreeDeprecated()| with update layout and
  // |computeVisibleSelectionInDOMTree()| to increase places hoisting update
  // layout.
  VisibleSelection ComputeVisibleSelectionInDOMTreeDeprecated() const;

  void SetSelection(const SelectionInDOMTree&, const SetSelectionOptions&);
  void SetSelectionAndEndTyping(const SelectionInDOMTree&);
  void SelectAll(SetSelectionBy);
  void SelectAll();
  void SelectSubString(const Element&, int offset, int count);
  void Clear();
  bool IsHidden() const;

  // TODO(tkent): These two functions were added to fix crbug.com/695211 without
  // changing focus behavior. Once we fix crbug.com/690272, we can remove these
  // functions.
  // setSelectionDeprecated() returns true if didSetSelectionDeprecated() should
  // be called.
  bool SetSelectionDeprecated(const SelectionInDOMTree&,
                              const SetSelectionOptions&);
  void DidSetSelectionDeprecated(const SelectionInDOMTree&,
                                 const SetSelectionOptions&);
  void SetSelectionForAccessibility(const SelectionInDOMTree&,
                                    const SetSelectionOptions&);

  // Call this after doing user-triggered selections to make it easy to delete
  // the frame you entirely selected.
  void SelectFrameElementInParentIfFullySelected();

  bool Contains(const PhysicalOffset&);

  bool Modify(SelectionModifyAlteration,
              SelectionModifyDirection,
              TextGranularity,
              SetSelectionBy);

  // Moves the selection extent based on the selection granularity strategy.
  // This function does not allow the selection to collapse. If the new
  // extent is resolved to the same position as the current base, this
  // function will do nothing.
  void MoveRangeSelectionExtent(const IntPoint&);
  void MoveRangeSelection(const IntPoint& base_point,
                          const IntPoint& extent_point,
                          TextGranularity);

  TextGranularity Granularity() const { return granularity_; }

  // Returns true if specified layout block should paint caret. This function is
  // called during painting only.
  bool ShouldPaintCaret(const LayoutBlock&) const;

  // Bounds of (possibly transformed) caret in absolute coords
  IntRect AbsoluteCaretBounds() const;

  // Returns anchor and focus bounds in absolute coords.
  // If the selection range is empty, returns the caret bounds.
  // Note: this updates styles and layout, use cautiously.
  bool ComputeAbsoluteBounds(IntRect& anchor, IntRect& focus) const;

  // Computes the rect we should use when scrolling/zooming a selection into
  // view.
  IntRect ComputeRectToScroll(RevealExtentOption);

  void DidChangeFocus();

  SelectionInDOMTree GetSelectionInDOMTree() const;
  bool IsDirectional() const;

  void DidAttachDocument(Document*);

  void DidLayout();
  void CommitAppearanceIfNeeded();
  void SetCaretEnabled(bool caret_is_visible);
  void ScheduleVisualUpdate() const;
  void ScheduleVisualUpdateForPaintInvalidationIfNeeded() const;

  // Paint invalidation methods delegating to FrameCaret.
  void LayoutBlockWillBeDestroyed(const LayoutBlock&);
  void UpdateStyleAndLayoutIfNeeded();
  void InvalidatePaint(const LayoutBlock&, const PaintInvalidatorContext&);

  void PaintCaret(GraphicsContext&, const PhysicalOffset&);

  // Used to suspend caret blinking while the mouse is down.
  void SetCaretBlinkingSuspended(bool);
  bool IsCaretBlinkingSuspended() const;

  // Focus
  bool SelectionHasFocus() const;
  void SetFrameIsFocused(bool);
  bool FrameIsFocused() const { return focused_; }
  bool FrameIsFocusedAndActive() const;
  void PageActivationChanged();

  bool IsHandleVisible() const { return is_handle_visible_; }
  bool ShouldShrinkNextTap() const { return should_shrink_next_tap_; }

  // Returns true if a word is selected.
  bool SelectWordAroundCaret();

#if DCHECK_IS_ON()
  void ShowTreeForThis() const;
#endif

  void SetFocusedNodeIfNeeded();
  void NotifyTextControlOfSelectionChange(SetSelectionBy);

  String SelectedHTMLForClipboard() const;
  String SelectedText(const TextIteratorBehavior&) const;
  String SelectedText() const;
  String SelectedTextForClipboard() const;

  // This returns last layouted selection bounds of LayoutSelection rather than
  // SelectionEditor keeps.
  PhysicalRect AbsoluteUnclippedBounds() const;

  // TODO(tkent): This function has a bug that scrolling doesn't work well in
  // a case of RangeSelection. crbug.com/443061
  void RevealSelection(
      const mojom::blink::ScrollAlignment& = ScrollAlignment::CenterIfNeeded(),
      RevealExtentOption = kDoNotRevealExtent);
  void SetSelectionFromNone();

  void UpdateAppearance();
  bool ShouldShowBlockCursor() const;
  void SetShouldShowBlockCursor(bool);

  void CacheRangeOfDocument(Range*);
  Range* DocumentCachedRange() const;
  void ClearDocumentCachedRange();

  // Invalidates the cached visual selection information, like
  // |VisibleSelection| and selection bounds.
  void MarkCacheDirty();

  FrameCaret& FrameCaretForTesting() const { return *frame_caret_; }

  LayoutTextSelectionStatus ComputeLayoutSelectionStatus(
      const LayoutText& text) const;
  LayoutSelectionStatus ComputeLayoutSelectionStatus(
      const NGInlineCursor& cursor) const;

  void Trace(Visitor*) const override;

 private:
  friend class CaretDisplayItemClientTest;
  friend class FrameSelectionTest;
  friend class PaintControllerPaintTestBase;
  friend class SelectionControllerTest;

  const CaretDisplayItemClient& CaretDisplayItemClientForTesting() const;

  void NotifyAccessibilityForSelectionChange();
  void NotifyCompositorForSelectionChange();
  void NotifyEventHandlerForSelectionChange();

  void FocusedOrActiveStateChanged();

  GranularityStrategy* GetGranularityStrategy();

  void MoveRangeSelectionInternal(const SelectionInDOMTree&, TextGranularity);

  // Implementation of |SynchronousMutationObserver| member functions.
  void ContextDestroyed() final;
  void NodeChildrenWillBeRemoved(ContainerNode&) final;
  void NodeWillBeRemoved(Node&) final;

  Member<LocalFrame> frame_;
  const Member<LayoutSelection> layout_selection_;
  const Member<SelectionEditor> selection_editor_;

  TextGranularity granularity_;
  LayoutUnit x_pos_for_vertical_arrow_navigation_;

  bool focused_ : 1;

  // The selection is currently being modified via the "Modify" method.
  bool is_being_modified_ = false;

  bool is_handle_visible_ = false;
  // TODO(editing-dev): We should change is_directional_ type to enum.
  // as directional can have three values forward, backward or directionless.
  bool is_directional_;
  bool should_shrink_next_tap_ = false;

  // Controls text granularity used to adjust the selection's extent in
  // moveRangeSelectionExtent.
  std::unique_ptr<GranularityStrategy> granularity_strategy_;

  const Member<FrameCaret> frame_caret_;

  DISALLOW_COPY_AND_ASSIGN(FrameSelection);
};

}  // namespace blink

#if DCHECK_IS_ON()
// Outside the blink namespace for ease of invocation from gdb.
void showTree(const blink::FrameSelection&);
void showTree(const blink::FrameSelection*);
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FRAME_SELECTION_H_
