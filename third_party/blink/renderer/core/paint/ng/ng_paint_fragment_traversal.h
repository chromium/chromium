// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_PAINT_FRAGMENT_TRAVERSAL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_PAINT_FRAGMENT_TRAVERSAL_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGPaintFragment;

// Utility class for traversing the paint fragment tree.
//
// This class has two groups of functions; one is a traversing cursor, by
// instantiating and using instance functions. The other is a set of static
// functions that are similar to DOM traversal functions.
class CORE_EXPORT NGPaintFragmentTraversal {
  STACK_ALLOCATED();

 public:
  NGPaintFragmentTraversal(const NGPaintFragmentTraversal& other);
  NGPaintFragmentTraversal(NGPaintFragmentTraversal&& other);
  NGPaintFragmentTraversal();
  NGPaintFragmentTraversal& operator=(const NGPaintFragmentTraversal& other);

  // Create an instance to traverse descendants of |root|.
  explicit NGPaintFragmentTraversal(const NGPaintFragment& root);

  // Create an instance to traverse descendants of |root|, starting at |start|.
  // Same as constructing with |root| and then |MoveTo()|.
  NGPaintFragmentTraversal(const NGPaintFragment& root,
                           const NGPaintFragment& start);

  bool IsAtEnd() const { return !current_; }
  explicit operator bool() const { return !IsAtEnd(); }

  const NGPaintFragment* get() const {
    DCHECK(current_);
    return current_;
  }
  const NGPaintFragment& operator*() const { return *get(); }
  const NGPaintFragment* operator->() const { return get(); }

  // Move to the specified fragment. The fragment must be a descendant of
  // |root|. This function is O(n) where |n| is the number of senior siblings
  // before |fragment|.
  void MoveTo(const NGPaintFragment& fragment);

  // Move to the next node using the pre-order depth-first-search.
  // Note: When |IsAtEnd()| is true, this function does nothing.
  void MoveToNext();

  // Move to the next sibling, or next ancestor node using the pre-order
  // depth-first-search, skipping children of the current node.
  void MoveToNextSiblingOrAncestor();

  // Move to the parent of current fragment. When |current_| is a child of
  // |root_|, this function makes |IsAtEnd()| to true.
  // Note: When |IsAtEnd()| is true, this function does nothing.
  void MoveToParent();

  // Move to the previous node using the pre-order depth-first-search. When
  // |current_| is the first child of |root_|, this function makes |IsAtEnd()|
  // to true.
  // Note: When |IsAtEnd()| is true, this function does nothing.
  void MoveToPrevious();

  // Returns the previous/next inline leaf fragment (text or atomic inline) of
  // the passed fragment, which itself must be inline.
  void MoveToPreviousInlineLeaf();
  void MoveToNextInlineLeaf();

  // Variants of the above two skipping line break fragments.
  void MoveToPreviousInlineLeafIgnoringLineBreak();
  void MoveToNextInlineLeafIgnoringLineBreak();

  //
  // Following functions are static, similar to DOM traversal utilities.
  //
  // Because fragments have children as a vector, not a two-way list, static
  // functions are not as cheap as their DOM versions. When traversing more than
  // once, instace functions are recommended.

  class AncestorRange final {
    STACK_ALLOCATED();

   public:
    class Iterator final
        : public std::iterator<std::forward_iterator_tag, NGPaintFragment*> {
      STACK_ALLOCATED();

     public:
      explicit Iterator(NGPaintFragment* current) : current_(current) {}

      NGPaintFragment* operator*() const { return operator->(); }
      NGPaintFragment* operator->() const;

      void operator++();

      bool operator==(const Iterator& other) const {
        return current_ == other.current_;
      }
      bool operator!=(const Iterator& other) const {
        return !operator==(other);
      }

     private:
      NGPaintFragment* current_;
    };

    explicit AncestorRange(const NGPaintFragment& start) : start_(&start) {}

    Iterator begin() const {
      return Iterator(const_cast<NGPaintFragment*>(start_));
    }
    Iterator end() const { return Iterator(nullptr); }

   private:
    const NGPaintFragment* const start_;
  };

  // Returns inclusive ancestors.
  static AncestorRange InclusiveAncestorsOf(const NGPaintFragment&);

  class CORE_EXPORT InlineDescendantsRange final {
    STACK_ALLOCATED();

   public:
    class CORE_EXPORT Iterator final
        : public std::iterator<std::forward_iterator_tag, NGPaintFragment*> {
      STACK_ALLOCATED();

     public:
      explicit Iterator(const NGPaintFragment& container);
      Iterator() = default;

      NGPaintFragment* operator*() const { return operator->(); }
      NGPaintFragment* operator->() const;

      void operator++();

      bool operator==(const Iterator& other) const {
        return current_ == other.current_;
      }
      bool operator!=(const Iterator& other) const {
        return !operator==(other);
      }

     private:
      NGPaintFragment* Next(const NGPaintFragment& fragment) const;
      static bool IsInlineFragment(const NGPaintFragment& fragment);
      static bool ShouldTraverse(const NGPaintFragment& fragment);

      const NGPaintFragment* container_ = nullptr;
      NGPaintFragment* current_ = nullptr;
    };

    explicit InlineDescendantsRange(const NGPaintFragment& container)
        : container_(&container) {}

    Iterator begin() const { return Iterator(*container_); }
    Iterator end() const { return Iterator(); }

   private:
    const NGPaintFragment* const container_;
  };

  // Returns inline descendants of |container| in preorder.
  static InlineDescendantsRange InlineDescendantsOf(
      const NGPaintFragment& container);

 private:
  void EnsureIndex();
  bool IsInlineLeaf() const;
  bool IsLineBreak() const;
  void MoveToFirstChild();
  void MoveToLastChild();
  void Reset();

  // |current_| holds a |NGPaintFragment| specified by |index|th child of
  // |parent| of the last element of |stack_|.
  const NGPaintFragment* current_ = nullptr;

  // The root of subtree where traversing is taken place. |root_| is excluded
  // from traversal. |current_| can't |root_|.
  const NGPaintFragment* root_ = nullptr;

  // Keep a list of siblings for MoveToPrevious().
  // TODO(kojii): We could keep a stack of this to avoid repetitive
  // constructions of the list when we move up/down levels. Also can consider
  // sharing with NGPaintFragmentTraversalContext.
  unsigned current_index_ = 0;
  Vector<NGPaintFragment*, 16> siblings_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_PAINT_FRAGMENT_TRAVERSAL_H_
