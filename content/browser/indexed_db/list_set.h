// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LIST_SET_H_
#define CONTENT_BROWSER_INDEXED_DB_LIST_SET_H_

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <list>
#include <memory>
#include <set>

#include "base/logging.h"

//
// A container class that provides fast containment test (like a set)
// but maintains insertion order for iteration (like list).
//
// Member types of value (primitives and objects by value), raw pointers
// and scoped_refptr<> are supported.
//
template <typename T>
class list_set {
 public:
  list_set() {}
  list_set(const list_set<T>& other) : list_(other.list_), set_(other.set_) {}
  list_set& operator=(const list_set<T>& other) {
    list_ = other.list_;
    set_ = other.set_;
    return *this;
  }

  void insert_front(const T& elem) {
    if (set_.find(elem) != set_.end())
      return;
    set_.insert(elem);
    list_.push_front(elem);
  }

  void insert(const T& elem) {
    if (set_.find(elem) != set_.end())
      return;
    set_.insert(elem);
    list_.push_back(elem);
  }

  void erase(const T& elem) {
    if (set_.find(elem) == set_.end())
      return;
    set_.erase(elem);
    typename std::list<T>::iterator it =
        std::find(list_.begin(), list_.end(), elem);
    DCHECK(it != list_.end());
    list_.erase(it);
  }

  void clear() {
    set_.clear();
    list_.clear();
  }

  size_t count(const T& elem) const {
    return set_.find(elem) == set_.end() ? 0 : 1;
  }

  size_t size() const {
    DCHECK_EQ(list_.size(), set_.size());
    return set_.size();
  }

  bool empty() const {
    DCHECK_EQ(list_.empty(), set_.empty());
    return set_.empty();
  }

  class const_iterator;

  class iterator {
   public:
    typedef iterator self_type;
    typedef T value_type;
    typedef value_type& reference;
    typedef value_type* pointer;
    typedef std::bidirectional_iterator_tag iterator_category;
    typedef std::ptrdiff_t difference_type;

    explicit iterator(typename std::list<T>::iterator it) : it_(it) {}
    self_type& operator++() {
      ++it_;
      return *this;
    }
    self_type operator++(int /*ignored*/) {
      self_type result(*this);
      ++(*this);
      return result;
    }
    self_type& operator--() {
      --it_;
      return *this;
    }
    self_type operator--(int /*ignored*/) {
      self_type result(*this);
      --(*this);
      return result;
    }
    reference operator*() const { return *it_; }
    pointer operator->() const { return &(*it_); }
    bool operator==(const iterator& rhs) const { return it_ == rhs.it_; }
    bool operator!=(const iterator& rhs) const { return it_ != rhs.it_; }

    inline operator const_iterator() const { return const_iterator(it_); }

   private:
    typename std::list<T>::iterator it_;
  };

  class const_iterator {
   public:
    typedef const_iterator self_type;
    typedef T value_type;
    typedef const value_type& reference;
    typedef const value_type* pointer;
    typedef std::bidirectional_iterator_tag iterator_category;
    typedef std::ptrdiff_t difference_type;

    explicit inline const_iterator(typename std::list<T>::const_iterator it)
        : it_(it) {}
    self_type& operator++() {
      ++it_;
      return *this;
    }
    self_type operator++(int ignored) {
      self_type result(*this);
      ++(*this);
      return result;
    }
    self_type& operator--() {
      --it_;
      return *this;
    }
    self_type operator--(int ignored) {
      self_type result(*this);
      --(*this);
      return result;
    }
    reference operator*() const { return *it_; }
    pointer operator->() const { return &(*it_); }
    bool operator==(const const_iterator& rhs) const { return it_ == rhs.it_; }
    bool operator!=(const const_iterator& rhs) const { return it_ != rhs.it_; }

   private:
    typename std::list<T>::const_iterator it_;
  };

  iterator begin() { return iterator(list_.begin()); }
  iterator end() { return iterator(list_.end()); }
  const_iterator begin() const { return const_iterator(list_.begin()); }
  const_iterator end() const { return const_iterator(list_.end()); }

 private:
  std::list<T> list_;
  std::set<T> set_;
};

// Prevent instantiation of list_set<std::unique_ptr<T>> as the current
// implementation would fail.
// TODO(jsbell): Support scoped_ptr through specialization.
template <typename T>
class list_set<std::unique_ptr<T>>;

#endif  // CONTENT_BROWSER_INDEXED_DB_LIST_SET_H_
