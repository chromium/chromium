// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_fragment_traversal.h"

#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

namespace blink {

namespace {

using Result = NGPhysicalFragmentWithOffset;

class NGPhysicalFragmentCollectorBase {
  STACK_ALLOCATED();

 public:
  virtual Vector<Result> CollectFrom(const NGPhysicalFragment&) = 0;
  NGPhysicalFragmentCollectorBase(const NGPhysicalFragmentCollectorBase&) =
      delete;
  NGPhysicalFragmentCollectorBase& operator=(
      const NGPhysicalFragmentCollectorBase&) = delete;

 protected:
  explicit NGPhysicalFragmentCollectorBase() = default;

  virtual void Visit() = 0;

  const NGPhysicalFragment& GetFragment() const { return *current_fragment_; }
  void SetShouldStopTraversing() { should_stop_traversing_ = true; }
  bool HasStoppedTraversing() const { return should_stop_traversing_; }

  void Emit() {
    results_.push_back(Result{current_fragment_, current_offset_to_root_});
  }

  // Visits and collets fragments in the subtree rooted at |fragment|.
  // |fragment| itself is not visited.
  Vector<Result> CollectExclusivelyFrom(const NGPhysicalFragment& fragment) {
    current_fragment_ = &fragment;
    root_fragment_ = &fragment;
    VisitChildren();
    return std::move(results_);
  }

  // Visits and collets fragments in the subtree rooted at |fragment|.
  // |fragment| itself is visited.
  Vector<Result> CollectInclusivelyFrom(const NGPhysicalFragment& fragment) {
    current_fragment_ = &fragment;
    root_fragment_ = &fragment;
    Visit();
    return std::move(results_);
  }

  void VisitChildren() {
    if (should_stop_traversing_)
      return;

    const NGPhysicalFragment& fragment = *current_fragment_;
    if (!fragment.IsContainer())
      return;

    // Traverse descendants unless the fragment is laid out separately from the
    // inline layout algorithm.
    if (&fragment != root_fragment_ && fragment.IsFormattingContextRoot())
      return;

    DCHECK(fragment.IsContainer());
    DCHECK(fragment.IsInline() || fragment.IsLineBox() ||
           (fragment.IsBlockFlow() &&
            To<NGPhysicalBoxFragment>(fragment).IsInlineFormattingContext()));

    for (const auto& child :
         To<NGPhysicalContainerFragment>(fragment).Children()) {
      base::AutoReset<PhysicalOffset> offset_resetter(
          &current_offset_to_root_, current_offset_to_root_ + child.Offset());
      base::AutoReset<const NGPhysicalFragment*> fragment_resetter(
          &current_fragment_, child.get());
      Visit();

      if (should_stop_traversing_)
        return;
    }
  }

 private:
  const NGPhysicalFragment* root_fragment_ = nullptr;
  const NGPhysicalFragment* current_fragment_ = nullptr;
  PhysicalOffset current_offset_to_root_;
  Vector<Result> results_;
  bool should_stop_traversing_ = false;
};

// The visitor emitting all visited fragments.
class DescendantCollector final : public NGPhysicalFragmentCollectorBase {
  STACK_ALLOCATED();

 public:
  DescendantCollector() = default;
  DescendantCollector(const DescendantCollector&) = delete;
  DescendantCollector& operator=(const DescendantCollector&) = delete;

  Vector<Result> CollectFrom(const NGPhysicalFragment& fragment) final {
    return CollectExclusivelyFrom(fragment);
  }

 private:
  void Visit() final {
    Emit();
    VisitChildren();
  }
};

// The visitor emitting fragments generated from the given LayoutInline,
// supporting culled inline.
// Note: Since we apply culled inline per line, we have a fragment for
// LayoutInline in second line but not in first line in
// "t0803-c5502-imrgn-r-01-b-ag.html".
class LayoutInlineCollector final : public NGPhysicalFragmentCollectorBase {
  STACK_ALLOCATED();

 public:
  explicit LayoutInlineCollector(const LayoutInline& container) {
    CollectInclusiveDescendants(container);
  }
  LayoutInlineCollector(const LayoutInlineCollector&) = delete;
  LayoutInlineCollector& operator=(const LayoutInlineCollector&) = delete;

  Vector<Result> CollectFrom(const NGPhysicalFragment& fragment) final {
    return CollectExclusivelyFrom(fragment);
  }

 private:
  void Visit() final {
    if (!GetFragment().IsLineBox() &&
        inclusive_descendants_.Contains(GetFragment().GetLayoutObject())) {
      Emit();
      return;
    }
    VisitChildren();
  }

  void CollectInclusiveDescendants(const LayoutInline& container) {
    inclusive_descendants_.insert(&container);
    for (const LayoutObject* node = container.FirstChild(); node;
         node = node->NextSibling()) {
      if (node->IsFloatingOrOutOfFlowPositioned())
        continue;
      if (node->IsBox() || node->IsText()) {
        inclusive_descendants_.insert(node);
        continue;
      }
      if (!node->IsLayoutInline())
        continue;
      CollectInclusiveDescendants(To<LayoutInline>(*node));
    }
  }

  HashSet<const LayoutObject*> inclusive_descendants_;
};

}  // namespace

// static
Vector<Result> NGInlineFragmentTraversal::SelfFragmentsOf(
    const NGPhysicalContainerFragment& container,
    const LayoutObject* layout_object) {
  if (const auto* layout_inline = DynamicTo<LayoutInline>(layout_object)) {
    // TODO(crbug.com/874361): Stop partial culling of inline boxes, so that we
    // can simply check existence of paint fragments below.
    if (!layout_inline->HasSelfPaintingLayer()) {
      return LayoutInlineCollector(To<LayoutInline>(*layout_object))
          .CollectFrom(container);
    }
  }
  Vector<Result> result;
  for (const NGPaintFragment* fragment :
       NGPaintFragment::InlineFragmentsFor(layout_object)) {
    result.push_back(Result{&fragment->PhysicalFragment(),
                            fragment->OffsetInContainerBlock()});
  }
  return result;
}

// static
Vector<Result> NGInlineFragmentTraversal::DescendantsOf(
    const NGPhysicalContainerFragment& container) {
  return DescendantCollector().CollectFrom(container);
}

}  // namespace blink
