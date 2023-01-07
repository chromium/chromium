// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_BPF_DSL_CONS_H_
#define SANDBOX_LINUX_BPF_DSL_CONS_H_

#include <memory>

#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace cons {

// Namespace cons provides an abstraction for immutable "cons list"
// data structures as commonly provided in functional programming
// languages like Lisp or Haskell.
//
// A cons list is a linked list consisting of "cells", each of which
// have a "head" and a "tail" element. A cell's head element contains
// a user specified value, while the tail element contains a (possibly
// null) pointer to another cell.
//
// An empty list (idiomatically referred to as "nil") can be
// constructed as "cons::List<Foo>()" or simply as "nullptr" if Foo
// can be inferred from context (e.g., calling a function that has a
// "cons::List<Foo>" parameter).
//
// Existing lists (including empty lists) can be extended by
// prepending new values to the front using the "Cons(head, tail)"
// function, which will allocate a new cons cell. Notably, cons lists
// support creating multiple lists that share a common tail sequence.
//
// Lastly, lists support iteration via C++11's range-based for loop
// construct.
//
// Examples:
//
//   // basic construction
//   const cons::List<char> kNil = nullptr;
//   cons::List<char> ba = Cons('b', Cons('a', kNil));
//
//   // common tail sequence
//   cons::List<char> cba = Cons('c', ba);
//   cons::List<char> dba = Cons('d', ba);
//
//   // iteration
//   for (const char& ch : cba) {
//     // iterates 'c', 'b', 'a'
//   }
//   for (const char& ch : dba) {
//     // iterates 'd', 'b', 'a'
//   }

// Forward declarations.
template <typename T>
class Cell;
template <typename T>
class ListIterator;

// List represents a (possibly null) pointer to a cons cell.
template <typename T>
using List = std::shared_ptr<const Cell<T>>;

// Cons extends a cons list by prepending a new value to the front.
template <typename T>
List<T> Cons(const T& head, List<T> tail) {
  return std::make_shared<Cell<T>>(head, std::move(tail));
}

// Cell represents an individual "cons cell" within a cons list.
template <typename T>
class Cell {
 public:
  Cell(const T& head, List<T> tail) : head_(head), tail_(std::move(tail)) {}

  Cell(const Cell&) = delete;
  Cell& operator=(const Cell&) = delete;

  // Head returns this cell's head element.
  const T& head() const { return head_; }

  // Tail returns this cell's tail element.
  const List<T>& tail() const { return tail_; }

 private:
  T head_;
  List<T> tail_;
};

// Begin returns a list iterator pointing to the first element of the
// cons list. It's provided to support range-based for loops.
template <typename T>
ListIterator<T> begin(const List<T>& list) {
  return ListIterator<T>(list);
}

// End returns a list iterator pointing to the "past-the-end" element
// of the cons list (i.e., nil). It's provided to support range-based
// for loops.
template <typename T>
ListIterator<T> end(const List<T>& list) {
  return ListIterator<T>();
}

// ListIterator provides C++ forward iterator semantics for traversing
// a cons list.
template <typename T>
class ListIterator {
 public:
  ListIterator() : list_() {}
  explicit ListIterator(const List<T>& list) : list_(list) {}

  const T& operator*() const { return list_->head(); }

  ListIterator& operator++() {
    list_ = list_->tail();
    return *this;
  }

  friend bool operator==(const ListIterator& lhs, const ListIterator& rhs) {
    return lhs.list_ == rhs.list_;
  }

 private:
  List<T> list_;
};

template <typename T>
bool operator!=(const ListIterator<T>& lhs, const ListIterator<T>& rhs) {
  return !(lhs == rhs);
}

}  // namespace cons
}  // namespace sandbox

#endif  // SANDBOX_LINUX_BPF_DSL_CONS_H_
