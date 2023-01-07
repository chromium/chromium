// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_COMMANDS_SELECTION_FOR_UNDO_STEP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_COMMANDS_SELECTION_FOR_UNDO_STEP_H_

#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// This class represents selection in |UndoStep|.
class SelectionForUndoStep final {
  DISALLOW_NEW();

 public:
  class Builder;

  // Returns newly constructed |SelectionForUndoStep| from |SelectionInDOMTree|
  // with computing direction of selection by base <= extent. Thus, computation
  // time depends O(depth of tree).
  static SelectionForUndoStep From(const SelectionInDOMTree&);

  SelectionForUndoStep(const SelectionForUndoStep&);
  SelectionForUndoStep();

  SelectionForUndoStep& operator=(const SelectionForUndoStep&);

  bool operator==(const SelectionForUndoStep&) const;
  bool operator!=(const SelectionForUndoStep&) const;

  TextAffinity Affinity() const { return affinity_; }
  Position Base() const { return base_; }
  Position Extent() const { return extent_; }
  bool IsBaseFirst() const { return is_base_first_; }
  Element* RootEditableElement() const { return root_editable_element_.Get(); }

  SelectionInDOMTree AsSelection() const;

  // Selection type predicates
  bool IsCaret() const;
  bool IsNone() const;
  bool IsRange() const;

  // Returns |base_| if |base_ <= extent| at construction time, otherwise
  // |extent_|.
  Position Start() const;
  // Returns |extent_| if |base_ <= extent| at construction time, otherwise
  // |base_|.
  Position End() const;

  bool IsValidFor(const Document&) const;

  void Trace(Visitor*) const;

 private:
  // |base_| and |extent_| can be disconnected from document.
  Position base_;
  Position extent_;
  TextAffinity affinity_ = TextAffinity::kDownstream;
  // Note: We should compute |is_base_first_| at construction otherwise we
  // fail "backward and forward delete" case in "undo-delete-boundary.html".
  bool is_base_first_ = true;
  // Since |base_| and |extent_| can be disconnected from document, we have to
  // calculate the root editable element at construction time
  Member<Element> root_editable_element_;
};

// Builds |SelectionForUndoStep| object with disconnected position. You should
// use |SelectionForUndoStep::From()| if positions are connected.
class SelectionForUndoStep::Builder final {
  DISALLOW_NEW();

 public:
  Builder();
  Builder(const Builder&) = delete;
  Builder& operator=(const Builder&) = delete;

  const SelectionForUndoStep& Build() const { return selection_; }

  // |base| and |extent| can be disconnected.
  Builder& SetBaseAndExtentAsBackwardSelection(const Position& base,
                                               const Position& extent);

  // |base| and |extent| can be disconnected.
  Builder& SetBaseAndExtentAsForwardSelection(const Position& base,
                                              const Position& extent);

  void Trace(Visitor*) const;

 private:
  SelectionForUndoStep selection_;
};

VisibleSelection CreateVisibleSelection(const SelectionForUndoStep&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_COMMANDS_SELECTION_FOR_UNDO_STEP_H_
