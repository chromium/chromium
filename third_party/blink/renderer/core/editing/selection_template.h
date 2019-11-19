// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SELECTION_TEMPLATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SELECTION_TEMPLATE_H_

#include <iosfwd>
#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_type.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// SelectionTemplate is used for representing a selection in DOM tree or Flat
// tree with template parameter |Strategy|. Instances of |SelectionTemplate|
// are "virtually" immutable objects, we change |SelectionTemplate| by copying
// in |SelectionEdtior| and |InvalidSelectionResetter|.
//
// To construct |SelectionTemplate| object, please use |Builder| class.
template <typename Strategy>
class SelectionTemplate final {
  DISALLOW_NEW();

 public:
  // |Builder| is a helper class for constructing |SelectionTemplate| object.
  class CORE_EXPORT Builder final {
    DISALLOW_NEW();

   public:
    explicit Builder(const SelectionTemplate&);
    Builder();

    SelectionTemplate Build() const;

    // Move selection to |base|. |base| can't be null.
    Builder& Collapse(const PositionTemplate<Strategy>& base);
    Builder& Collapse(const PositionWithAffinityTemplate<Strategy>& base);

    // Extend selection to |extent|. It is error if selection is none.
    // |extent| can be in different tree scope of base, but should be in same
    // document.
    Builder& Extend(const PositionTemplate<Strategy>& extent);

    // Select all children in |node|.
    Builder& SelectAllChildren(const Node& /* node */);

    // Note: Since collapsed selection is a forward selection, we can't use
    // |SetAsBackwardSelection()| for collapsed range.
    Builder& SetAsBackwardSelection(const EphemeralRangeTemplate<Strategy>&);
    Builder& SetAsForwardSelection(const EphemeralRangeTemplate<Strategy>&);

    Builder& SetBaseAndExtent(const EphemeralRangeTemplate<Strategy>&);

    // |extent| can not be null if |base| isn't null.
    Builder& SetBaseAndExtent(const PositionTemplate<Strategy>& base,
                              const PositionTemplate<Strategy>& extent);

    // |extent| can be non-null while |base| is null, which is undesired.
    // Note: This function should be used only in "core/editing/commands".
    // TODO(yosin): Once all we get rid of all call sites, we remove this.
    Builder& SetBaseAndExtentDeprecated(
        const PositionTemplate<Strategy>& base,
        const PositionTemplate<Strategy>& extent);

    Builder& SetAffinity(TextAffinity);

   private:
    SelectionTemplate selection_;

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  // Resets selection at end of life time of the object when base and extent
  // are disconnected or moved to another document.
  class InvalidSelectionResetter final {
    DISALLOW_NEW();

   public:
    explicit InvalidSelectionResetter(const SelectionTemplate&);
    ~InvalidSelectionResetter();

    void Trace(Visitor*);

   private:
    const Member<const Document> document_;
    SelectionTemplate& selection_;

    DISALLOW_COPY_AND_ASSIGN(InvalidSelectionResetter);
  };

  SelectionTemplate(const SelectionTemplate& other);
  SelectionTemplate();

  SelectionTemplate& operator=(const SelectionTemplate&) = default;

  bool operator==(const SelectionTemplate&) const;
  bool operator!=(const SelectionTemplate&) const;

  PositionTemplate<Strategy> Base() const;
  PositionTemplate<Strategy> Extent() const;
  TextAffinity Affinity() const { return affinity_; }
  bool IsBaseFirst() const;
  bool IsCaret() const;
  bool IsNone() const { return base_.IsNull(); }
  bool IsRange() const;

  // Returns true if |this| selection holds valid values otherwise it causes
  // assertion failure.
  bool AssertValid() const;
  bool AssertValidFor(const Document&) const;

  PositionTemplate<Strategy> ComputeEndPosition() const;
  PositionTemplate<Strategy> ComputeStartPosition() const;
  EphemeralRangeTemplate<Strategy> ComputeRange() const;

  // Returns |SelectionType| for |this| based on |base_| and |extent_|.
  SelectionType Type() const;

  void Trace(Visitor*);

  void PrintTo(std::ostream*, const char* type) const;
#if DCHECK_IS_ON()
  void ShowTreeForThis() const;
#endif

 private:
  friend class SelectionEditor;

  enum class Direction {
    kNotComputed,
    kForward,   // base <= extent
    kBackward,  // base > extent
  };

  Document* GetDocument() const;
  bool IsValidFor(const Document&) const;
  void ResetDirectionCache() const;

  PositionTemplate<Strategy> base_;
  PositionTemplate<Strategy> extent_;
  TextAffinity affinity_ = TextAffinity::kDownstream;
  mutable Direction direction_ = Direction::kForward;
#if DCHECK_IS_ON()
  uint64_t dom_tree_version_;
#endif
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    SelectionTemplate<EditingStrategy>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    SelectionTemplate<EditingInFlatTreeStrategy>;

using SelectionInDOMTree = SelectionTemplate<EditingStrategy>;
using SelectionInFlatTree = SelectionTemplate<EditingInFlatTreeStrategy>;

CORE_EXPORT SelectionInDOMTree
ConvertToSelectionInDOMTree(const SelectionInFlatTree&);
CORE_EXPORT SelectionInFlatTree
ConvertToSelectionInFlatTree(const SelectionInDOMTree&);

CORE_EXPORT std::ostream& operator<<(std::ostream&, const SelectionInDOMTree&);
CORE_EXPORT std::ostream& operator<<(std::ostream&, const SelectionInFlatTree&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_SELECTION_TEMPLATE_H_
