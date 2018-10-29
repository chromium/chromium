// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGPaintFragmentTraversal_h
#define NGPaintFragmentTraversal_h

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LayoutObject;
class NGPaintFragment;

// Used for return value of traversing fragment tree.
struct CORE_EXPORT NGPaintFragmentWithContainerOffset {
  DISALLOW_NEW();
  NGPaintFragment* fragment;
  // Offset relative to container fragment
  NGPhysicalOffset container_offset;
};

// Represents an NGPaintFragment by its parent and its index in the parent's
// |Children()| vector.
struct CORE_EXPORT NGPaintFragmentTraversalContext {
  STACK_ALLOCATED();

 public:
  NGPaintFragmentTraversalContext() = default;
  explicit NGPaintFragmentTraversalContext(const NGPaintFragment* fragment);
  NGPaintFragmentTraversalContext(const NGPaintFragment* parent,
                                  unsigned index);
  // TODO(kojii): deprecated, prefer constructors to avoid unexpected
  // instantiation.
  static NGPaintFragmentTraversalContext Create(const NGPaintFragment*);

  bool IsNull() const { return !parent; }
  const NGPaintFragment* GetFragment() const;

  bool operator==(const NGPaintFragmentTraversalContext& other) const {
    return parent == other.parent && index == other.index;
  }

  const NGPaintFragment* parent = nullptr;
  unsigned index = 0;
  Vector<NGPaintFragment*, 16> siblings;
};

// Utility class for traversing the paint fragment tree.
//
// This class has two groups of functions; one is a traversing cursor, by
// instantiating and using instance functions. The other is a set of static
// functions that are similar to DOM traversal functions.
class CORE_EXPORT NGPaintFragmentTraversal {
  STACK_ALLOCATED();

 public:
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

  // Returns descendants without paint layer in preorder.
  static Vector<NGPaintFragmentWithContainerOffset> DescendantsOf(
      const NGPaintFragment&);

  // Returns inline descendants in preorder.
  static Vector<NGPaintFragmentWithContainerOffset> InlineDescendantsOf(
      const NGPaintFragment&);

  static Vector<NGPaintFragmentWithContainerOffset> SelfFragmentsOf(
      const NGPaintFragment&,
      const LayoutObject* target);

  // Returns the line box paint fragment of |line|. |line| itself must be the
  // paint fragment of a line box.
  static NGPaintFragment* PreviousLineOf(const NGPaintFragment& line);

  // Returns the previous/next inline leaf fragment (text or atomic inline)of
  // the passed fragment, which itself must be inline.
  static NGPaintFragmentTraversalContext PreviousInlineLeafOf(
      const NGPaintFragmentTraversalContext&);
  static NGPaintFragmentTraversalContext NextInlineLeafOf(
      const NGPaintFragmentTraversalContext&);

  // Variants of the above two skipping line break fragments.
  static NGPaintFragmentTraversalContext PreviousInlineLeafOfIgnoringLineBreak(
      const NGPaintFragmentTraversalContext&);
  static NGPaintFragmentTraversalContext NextInlineLeafOfIgnoringLineBreak(
      const NGPaintFragmentTraversalContext&);

 private:
  // |current_| holds a |NGPaintFragment| specified by |index|th child of
  // |parent| of the last element of |stack_|.
  const NGPaintFragment* current_ = nullptr;

  // The root of subtree where traversing is taken place. |root_| is excluded
  // from traversal. |current_| can't |root_|.
  const NGPaintFragment& root_;

  // Keep a list of siblings for MoveToPrevious().
  // TODO(kojii): We could keep a stack of this to avoid repetitive
  // constructions of the list when we move up/down levels. Also can consider
  // sharing with NGPaintFragmentTraversalContext.
  unsigned current_index_ = 0;
  Vector<NGPaintFragment*, 16> siblings_;

  DISALLOW_COPY_AND_ASSIGN(NGPaintFragmentTraversal);
};

}  // namespace blink

#endif  // NGPaintFragmentTraversal_h
