// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

namespace blink {

namespace {

// ------ Helpers for traversing inline fragments ------

unsigned IndexOf(const Vector<NGPaintFragment*, 16>& fragments,
                 const NGPaintFragment& fragment) {
  auto* const* it = std::find_if(
      fragments.begin(), fragments.end(),
      [&fragment](const auto& child) { return &fragment == child; });
  DCHECK(it != fragments.end());
  return static_cast<unsigned>(std::distance(fragments.begin(), it));
}

}  // namespace

NGPaintFragmentTraversal::NGPaintFragmentTraversal(const NGPaintFragment& root)
    : current_(root.FirstChild()), root_(&root) {}

NGPaintFragmentTraversal::NGPaintFragmentTraversal(const NGPaintFragment& root,
                                                   const NGPaintFragment& start)
    : root_(&root) {
  MoveTo(start);
}

NGPaintFragmentTraversal::NGPaintFragmentTraversal(
    const NGPaintFragmentTraversal& other)
    : current_(other.current_),
      root_(other.root_),
      current_index_(other.current_index_),
      siblings_(other.siblings_) {}

NGPaintFragmentTraversal::NGPaintFragmentTraversal(
    NGPaintFragmentTraversal&& other)
    : current_(other.current_),
      root_(other.root_),
      current_index_(other.current_index_),
      siblings_(std::move(other.siblings_)) {
  other.current_ = nullptr;
}

NGPaintFragmentTraversal::NGPaintFragmentTraversal() = default;

NGPaintFragmentTraversal& NGPaintFragmentTraversal::operator=(
    const NGPaintFragmentTraversal& other) {
  current_ = other.current_;
  root_ = other.root_;
  current_index_ = other.current_index_;
  siblings_ = other.siblings_;
  return *this;
}

void NGPaintFragmentTraversal::EnsureIndex() {
  current_->Parent()->Children().ToList(&siblings_);
  auto** const it =
      std::find_if(siblings_.begin(), siblings_.end(),
                   [this](const auto& child) { return current_ == child; });
  DCHECK(it != siblings_.end());
  current_index_ = static_cast<unsigned>(std::distance(siblings_.begin(), it));
}

void NGPaintFragmentTraversal::Reset() {
  current_ = nullptr;
  current_index_ = 0;
  siblings_.Shrink(0);
}

void NGPaintFragmentTraversal::MoveTo(const NGPaintFragment& fragment) {
  DCHECK(fragment.IsDescendantOfNotSelf(*root_));
  current_ = &fragment;
}

void NGPaintFragmentTraversal::MoveToNext() {
  if (IsAtEnd())
    return;

  if (const NGPaintFragment* first_child = current_->FirstChild())
    return MoveToFirstChild();
  MoveToNextSiblingOrAncestor();
}

void NGPaintFragmentTraversal::MoveToNextSiblingOrAncestor() {
  while (!IsAtEnd()) {
    // Check if we have a next sibling.
    if (const NGPaintFragment* next = current_->NextSibling()) {
      current_ = next;
      if (!siblings_.IsEmpty()) {
        ++current_index_;
        return;
      }
      EnsureIndex();
      return;
    }

    MoveToParent();
  }
}

void NGPaintFragmentTraversal::MoveToParent() {
  if (IsAtEnd())
    return;

  current_ = current_->Parent();
  if (current_ == root_)
    current_ = nullptr;
  if (UNLIKELY(!siblings_.IsEmpty()))
    siblings_.Shrink(0);
}

void NGPaintFragmentTraversal::MoveToPrevious() {
  if (IsAtEnd())
    return;

  if (siblings_.IsEmpty()) {
    current_->Parent()->Children().ToList(&siblings_);
    current_index_ = IndexOf(siblings_, *current_);
  }

  if (!current_index_) {
    // There is no previous sibling of |current_|. We move to parent.
    MoveToParent();
    return;
  }

  current_ = siblings_[--current_index_];
  while (current_->FirstChild())
    MoveToLastChild();
}

NGPaintFragmentTraversal::AncestorRange
NGPaintFragmentTraversal::InclusiveAncestorsOf(const NGPaintFragment& start) {
  return AncestorRange(start);
}

NGPaintFragmentTraversal::InlineDescendantsRange
NGPaintFragmentTraversal::InlineDescendantsOf(
    const NGPaintFragment& container) {
  return InlineDescendantsRange(container);
}

void NGPaintFragmentTraversal::MoveToFirstChild() {
  DCHECK(current_->FirstChild());
  current_ = current_->FirstChild();
  current_index_ = 0;
  if (UNLIKELY(!siblings_.IsEmpty()))
    siblings_.Shrink(0);
}

void NGPaintFragmentTraversal::MoveToLastChild() {
  DCHECK(current_->FirstChild());
  current_->Children().ToList(&siblings_);
  DCHECK(!siblings_.IsEmpty());
  current_index_ = siblings_.size() - 1;
  current_ = siblings_[current_index_];
}

void NGPaintFragmentTraversal::MoveToNextInlineLeaf() {
  while (!IsAtEnd() && !IsInlineLeaf())
    MoveToNext();
  do {
    MoveToNext();
  } while (!IsAtEnd() && !IsInlineLeaf());
}

void NGPaintFragmentTraversal::MoveToPreviousInlineLeaf() {
  while (!IsAtEnd() && !IsInlineLeaf())
    MoveToPrevious();
  do {
    MoveToPrevious();
  } while (!IsAtEnd() && !IsInlineLeaf());
}

void NGPaintFragmentTraversal::MoveToPreviousInlineLeafIgnoringLineBreak() {
  do {
    MoveToPreviousInlineLeaf();
  } while (!IsAtEnd() && IsLineBreak());
}

void NGPaintFragmentTraversal::MoveToNextInlineLeafIgnoringLineBreak() {
  do {
    MoveToNextInlineLeaf();
  } while (!IsAtEnd() && IsLineBreak());
}

bool NGPaintFragmentTraversal::IsInlineLeaf() const {
  if (!current_->PhysicalFragment().IsInline())
    return false;
  return current_->PhysicalFragment().IsText() ||
         current_->PhysicalFragment().IsAtomicInline();
}

bool NGPaintFragmentTraversal::IsLineBreak() const {
  DCHECK(current_->PhysicalFragment().IsInline());
  auto* physical_text_fragment =
      DynamicTo<NGPhysicalTextFragment>(current_->PhysicalFragment());
  if (!physical_text_fragment)
    return false;
  return physical_text_fragment->IsLineBreak();
}

// ----
NGPaintFragment* NGPaintFragmentTraversal::AncestorRange::Iterator::operator->()
    const {
  DCHECK(current_);
  return current_;
}

void NGPaintFragmentTraversal::AncestorRange::Iterator::operator++() {
  DCHECK(current_);
  current_ = current_->Parent();
}

// ----

NGPaintFragmentTraversal::InlineDescendantsRange::Iterator::Iterator(
    const NGPaintFragment& container)
    : container_(&container), current_(container.FirstChild()) {
  if (!current_ || IsInlineFragment(*current_))
    return;
  operator++();
}

NGPaintFragment* NGPaintFragmentTraversal::InlineDescendantsRange::Iterator::
operator->() const {
  DCHECK(current_);
  return current_;
}

void NGPaintFragmentTraversal::InlineDescendantsRange::Iterator::operator++() {
  DCHECK(current_);
  do {
    current_ = Next(*current_);
  } while (current_ && !IsInlineFragment(*current_));
}

// static
// Returns next fragment of |start| in DFS pre-order within |container_| and
// skipping descendants in block formatting context.
NGPaintFragment*
NGPaintFragmentTraversal::InlineDescendantsRange::Iterator::Next(
    const NGPaintFragment& start) const {
  if (ShouldTraverse(start) && start.FirstChild())
    return start.FirstChild();
  for (NGPaintFragment* runner = const_cast<NGPaintFragment*>(&start);
       runner != container_; runner = runner->Parent()) {
    if (NGPaintFragment* next_sibling = runner->NextSibling())
      return next_sibling;
  }
  return nullptr;
}

// static
bool NGPaintFragmentTraversal::InlineDescendantsRange::Iterator::
    IsInlineFragment(const NGPaintFragment& fragment) {
  return fragment.PhysicalFragment().IsInline() ||
         fragment.PhysicalFragment().IsLineBox();
}

// static
bool NGPaintFragmentTraversal::InlineDescendantsRange::Iterator::ShouldTraverse(
    const NGPaintFragment& fragment) {
  return fragment.PhysicalFragment().IsContainer() &&
         !fragment.PhysicalFragment().IsBlockFormattingContextRoot();
}

}  // namespace blink
