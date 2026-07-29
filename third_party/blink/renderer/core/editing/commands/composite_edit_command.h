/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_COMMANDS_COMPOSITE_EDIT_COMMAND_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_COMMANDS_COMPOSITE_EDIT_COMMAND_H_

#include <optional>
#include <utility>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/editing/commands/edit_command.h"
#include "third_party/blink/renderer/core/editing/commands/editing_state.h"
#include "third_party/blink/renderer/core/editing/commands/undo_step.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DataTransfer;
class DeleteSelectionOptions;
class DocumentFragment;
class EditingStyle;
class Element;
class HTMLBRElement;
class HTMLElement;
class HTMLSpanElement;
class Text;

class CORE_EXPORT CompositeEditCommand : public EditCommand {
 public:
  enum ShouldPreserveSelection { kPreserveSelection, kDoNotPreserveSelection };
  enum ShouldPreserveStyle { kPreserveStyle, kDoNotPreserveStyle };

  ~CompositeEditCommand() override;

  const SelectionForUndoStep& StartingSelection() const {
    return starting_selection_;
  }
  const SelectionForUndoStep& EndingSelection() const {
    return ending_selection_;
  }

  // Raw-DOM lane: never VP-canonicalized at command birth. Migrated
  // commands prefer these accessors under EditingUseDomPositionApi so
  // they see the selection as the user authored it, not the layout-
  // canonicalized form. Unmigrated commands keep using the legacy
  // accessors above and observe no behavior change.
  const SelectionForUndoStep& StartingDomSelection() const {
    return starting_dom_selection_;
  }
  const SelectionForUndoStep& EndingDomSelection() const {
    return ending_dom_selection_;
  }

  void SetStartingSelection(const SelectionForUndoStep&);
  void SetEndingSelection(const SelectionForUndoStep&);

  // Raw-DOM lane setters. Migrated commands call these to update the
  // dom lane without going through the legacy (VP-aware) setter path.
  void SetStartingDomSelection(const SelectionForUndoStep&);
  void SetEndingDomSelection(const SelectionForUndoStep&);

  void SetParent(CompositeEditCommand*) override;

  // Returns |false| if the command failed.  e.g. It's aborted.
  bool Apply();
  bool IsFirstCommand(EditCommand* command) {
    return !commands_.empty() && commands_.front() == command;
  }
  UndoStep* GetUndoStep() { return undo_step_.Get(); }
  UndoStep* EnsureUndoStep();
  // Append undo step from an already applied command.
  void AppendCommandToUndoStep(CompositeEditCommand*);

  virtual bool IsReplaceSelectionCommand() const;
  virtual bool IsTypingCommand() const;
  virtual bool IsCommandGroupWrapper() const;
  virtual bool IsDragAndDropCommand() const;
  virtual bool PreservesTypingStyle() const;

  virtual void AppliedEditing();

  void Trace(Visitor*) const override;

 protected:
  explicit CompositeEditCommand(Document&, DataTransfer* = nullptr);

  VisibleSelection EndingVisibleSelection() const;
  //
  // sugary-sweet convenience functions to help create and apply edit commands
  // in composite commands
  //
  void AppendNode(Node*, ContainerNode* parent, EditingState*);
  void ApplyCommandToComposite(EditCommand*, EditingState*);
  void ApplyStyle(const EditingStyle*, EditingState*);
  void ApplyStyle(const EditingStyle*,
                  const Position& start,
                  const Position& end,
                  EditingState*);
  void ApplyStyledElement(Element*, EditingState*);
  void RemoveStyledElement(Element*, EditingState*);
  // Returns |false| if the EditingState has been aborted.
  bool DeleteSelection(EditingState*, const DeleteSelectionOptions&);
  virtual void DeleteTextFromNode(Text*, wtf_size_t offset, wtf_size_t count);
  bool IsRemovableBlock(const Node*);
  void InsertNodeAfter(Node*, Node* ref_child, EditingState*);
  // Insert nodes starting `insert_first_child` after `ref_child`.
  void InsertNodeListAfter(Node& insert_first_child,
                           Node& ref_child,
                           EditingState* editing_state);
  void InsertNodeAt(Node*, const Position&, EditingState*);
  void InsertNodeAtTabSpanPosition(Node*, const Position&, EditingState*);
  void InsertNodeBefore(Node*,
                        Node* ref_child,
                        EditingState*,
                        ShouldAssumeContentIsAlwaysEditable =
                            ShouldAssumeContentIsAlwaysEditable(false));
  void InsertParagraphSeparator(
      EditingState*,
      bool use_default_paragraph_element = false,
      bool paste_blockqutoe_into_unquoted_area = false);
  void InsertTextIntoNode(Text*,
                          wtf_size_t offset,
                          const String& text,
                          PasswordEchoBehavior);
  void MergeIdenticalElements(Element*, Element*, EditingState*);
  void RebalanceWhitespace();
  void RebalanceWhitespaceAt(const Position&);
  void RebalanceWhitespaceOnTextSubstring(Text*,
                                          wtf_size_t start_offset,
                                          wtf_size_t end_offset);
  void PrepareWhitespaceAtPositionForSplit(Position&);
  void ReplaceCollapsibleWhitespaceWithNonBreakingSpaceIfNeeded(
      const VisiblePosition&);
  void ReplaceCollapsibleWhitespaceWithNonBreakingSpaceIfNeeded(
      const Position&);
  bool CanRebalance(const Position&) const;
  void RemoveCssProperty(Element*, CSSPropertyID);
  void RemoveElementAttribute(Element*, const QualifiedName& attribute);
  // Remove all children if possible
  void RemoveAllChildrenIfPossible(
      ContainerNode*,
      EditingState*,
      ShouldAssumeContentIsAlwaysEditable =
          ShouldAssumeContentIsAlwaysEditable(false));
  void RemoveChildrenInRange(Node*,
                             wtf_size_t from,
                             wtf_size_t to,
                             EditingState*);
  virtual void RemoveNode(Node*,
                          EditingState*,
                          ShouldAssumeContentIsAlwaysEditable =
                              ShouldAssumeContentIsAlwaysEditable(false));
  HTMLSpanElement* ReplaceElementWithSpanPreservingChildrenAndAttributes(
      HTMLElement*);
  void RemoveNodePreservingChildren(
      Node*,
      EditingState*,
      ShouldAssumeContentIsAlwaysEditable =
          ShouldAssumeContentIsAlwaysEditable(false));
  void RemoveNodeAndPruneAncestors(Node*,
                                   EditingState*,
                                   Node* exclude_node = nullptr);
  void MoveRemainingSiblingsToNewParent(Node*,
                                        Node* past_last_node_to_move,
                                        Element* new_parent,
                                        EditingState*);
  void UpdatePositionForNodeRemovalPreservingChildren(Position&, Node&);
  void Prune(Node*, EditingState*, Node* exclude_node = nullptr);
  void ReplaceTextInNode(Text*,
                         wtf_size_t offset,
                         wtf_size_t count,
                         const String& replacement_text,
                         PasswordEchoBehavior);
  Position ReplaceSelectedTextInNode(const String&, PasswordEchoBehavior);
  Position PositionOutsideTabSpan(const Position&);
  void SetNodeAttribute(Element*,
                        const QualifiedName& attribute,
                        const AtomicString& value);
  void SplitElement(Element*, Node* at_child);
  void SplitTextNode(Text*, wtf_size_t offset);
  void SplitTextNodeContainingElement(Text*, wtf_size_t offset);
  void WrapContentsInDummySpan(Element*);

  void DeleteInsignificantText(Text*, wtf_size_t start, wtf_size_t end);
  void DeleteInsignificantText(const Position& start, const Position& end);
  void DeleteInsignificantTextDownstream(const Position&);

  HTMLBRElement* AppendBlockPlaceholder(Element*, EditingState*);
  HTMLBRElement* InsertBlockPlaceholder(const Position&, EditingState*);
  HTMLBRElement* AddBlockPlaceholderIfNeeded(Element*, EditingState*);
  void RemovePlaceholderAt(const Position&);

  HTMLElement* InsertNewDefaultParagraphElementAt(const Position&,
                                                  EditingState*);

  HTMLElement* MoveParagraphContentsToNewBlockIfNecessary(const Position&,
                                                          EditingState*);

  void PushAnchorElementDown(Element*, EditingState*);

  void MoveParagraph(const VisiblePosition&,
                     const VisiblePosition&,
                     const VisiblePosition&,
                     EditingState*,
                     ShouldPreserveSelection = kDoNotPreserveSelection,
                     ShouldPreserveStyle = kPreserveStyle,
                     Node* constraining_ancestor = nullptr);
  void MoveParagraphs(const VisiblePosition&,
                      const VisiblePosition&,
                      const VisiblePosition&,
                      EditingState*,
                      ShouldPreserveSelection = kDoNotPreserveSelection,
                      ShouldPreserveStyle = kPreserveStyle,
                      Node* constraining_ancestor = nullptr);
  void MoveParagraphWithClones(
      const VisiblePosition& start_of_paragraph_to_move,
      const VisiblePosition& end_of_paragraph_to_move,
      HTMLElement* block_element,
      Node* outer_node,
      EditingState*);
  void CloneParagraphUnderNewElement(const Position& start,
                                     const Position& end,
                                     Node* outer_node,
                                     Element* block_element,
                                     EditingState*);
  void CleanupAfterDeletion(EditingState*, VisiblePosition destination);
  void CleanupAfterDeletion(EditingState*, const Position& destination);
  void CleanupAfterDeletion(EditingState*);

  bool BreakOutOfEmptyListItem(EditingState*);
  bool BreakOutOfEmptyMailBlockquotedParagraph(EditingState*);

  Position PositionAvoidingSpecialElementBoundary(const Position&,
                                                  EditingState*);

  Node* SplitTreeToNode(Node*, Node*, bool split_ancestor = false);

  static bool IsNodeVisiblyContainedWithin(Node&, const EphemeralRange&);

  HeapVector<Member<EditCommand>> commands_;
  // The data transfer will be used for the input event
  // on contenteditables.
  Member<DataTransfer> data_transfer_;

 private:
  // Inserts a placeholder <br> at |before_paragraph| when pruning collapsed
  // adjacent paragraphs onto the same line. Canonicalizes the boundary
  // positions through VisiblePosition before testing paragraph boundaries.
  void InsertPlaceholderBrIfPruningCollapsed(const Position& before_paragraph,
                                             const Position& after_paragraph,
                                             EditingState* editing_state);

  // Returns true when the move may proceed to the paste phase. Verifies that
  // both `destination` and the post-delete ending selection still have
  // editable roots; returns false (without aborting) to signal the caller to
  // bail out silently.
  bool DestinationStillEditableForPaste(const VisiblePosition& destination);

  // Returns the plain-text offset of `destination` from the document root,
  // selecting the TextIteratorBehavior based on the EnterInOpenShadowRoots
  // flag. Requires clean layout.
  wtf_size_t ComputeDestinationIndex(const VisiblePosition& destination);

  // Sets the ending selection to `destination` (mirroring into the raw-DOM
  // lane when EditingUseDomPositionApi is enabled) and runs the
  // ReplaceSelectionCommand for `fragment`. Returns false when the caller
  // should return early; `editing_state` is aborted in the cases that
  // previously called ABORT_EDITING_COMMAND_IF or editing_state->Abort()
  // inline.
  bool SetDestinationSelectionAndPasteFragment(
      const VisiblePosition& destination,
      DocumentFragment* fragment,
      ShouldPreserveStyle should_preserve_style,
      EditingState* editing_state);

  // Restores the final ending selection from plain-text offsets
  // `destination_index + start_index` and `destination_index + end_index`
  // relative to `document_element`. Silently no-ops when either offset
  // cannot be reconstituted (collapsed-whitespace edge cases). Mirrors into
  // the raw-DOM lane when EditingUseDomPositionApi is enabled.
  void RestoreSelectionFromPlainText(wtf_size_t destination_index,
                                     wtf_size_t start_index,
                                     wtf_size_t end_index,
                                     Element& document_element);

  bool IsCompositeEditCommand() const final { return true; }

  // Helpers extracted from MoveParagraphs. VP callers compute the inputs
  // (Position via `.DeepEquivalent()`).
  std::pair<Position, Position> ComputeNormalizedMoveRange(
      const Position& start_of_paragraph,
      const Position& end_of_paragraph);
  // Returns preserved endpoints from the VisiblePosition lane, or std::nullopt
  // when nothing should be preserved.
  std::optional<std::pair<Position, Position>>
  ComputePreservedVisibleSelectionEndpoints(
      ShouldPreserveSelection should_preserve_selection);
  // Returns preserved endpoints from the raw-DOM lane, or std::nullopt when
  // nothing should be preserved or endpoints are stale.
  std::optional<std::pair<Position, Position>>
  ComputePreservedDomSelectionEndpoints(
      ShouldPreserveSelection should_preserve_selection);
  // True when a raw-DOM endpoint is non-null and still valid in this document.
  bool IsPreservedSelectionEndpointUsable(const Position& position) const;
  // Computes selection offsets relative to paragraph start, or std::nullopt
  // when selection is entirely outside the moved paragraph.
  std::optional<std::pair<wtf_size_t, wtf_size_t>>
  ComputePreservedSelectionIndices(const Position& start_of_paragraph,
                                   const Position& end_of_paragraph,
                                   const Position& selection_start,
                                   const Position& selection_end);
  EditingStyle* CaptureStyleInEmptyParagraph(
      const Position& start_of_paragraph);
  // Sets the ending selection to the delete range [start, end]. Mirrors into
  // the raw-DOM lane when EditingUseDomPositionApi is enabled.
  void SetEndingSelectionToDelete(const Position& start, const Position& end);

  SelectionForUndoStep starting_selection_;
  SelectionForUndoStep ending_selection_;
  // Raw-DOM lane mirroring starting_/ending_selection_.
  SelectionForUndoStep starting_dom_selection_;
  SelectionForUndoStep ending_dom_selection_;
  Member<UndoStep> undo_step_;
};

template <>
struct DowncastTraits<CompositeEditCommand> {
  static bool AllowFrom(const EditCommand& command) {
    return command.IsCompositeEditCommand();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_COMMANDS_COMPOSITE_EDIT_COMMAND_H_
