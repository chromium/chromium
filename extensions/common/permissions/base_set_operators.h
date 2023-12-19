// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_BASE_SET_OPERATORS_H_
#define EXTENSIONS_COMMON_PERMISSIONS_BASE_SET_OPERATORS_H_

#include <stddef.h>

#include <iterator>
#include <map>
#include <memory>

#include "base/check.h"

namespace extensions {

// Traits for template paramater of |BaseSetOperators<T>|. Specializations
// should define |ElementType| for the type of elements to store in the set,
// and |EmementIDType| for the type of element identifiers.
template <typename T>
struct BaseSetOperatorsTraits {};

// Set operations shared by |APIPermissionSet| and |ManifestPermissionSet|.
//
// TODO(rpaquay): It would be nice to remove the need for the sub-classes and
// instead directly use this class where needed.
template <typename T>
class BaseSetOperators {
 public:
  using ElementType = typename BaseSetOperatorsTraits<T>::ElementType;
  using ElementIDType = typename BaseSetOperatorsTraits<T>::ElementIDType;

  using Map = std::map<ElementIDType, std::unique_ptr<ElementType>>;

  class const_iterator {
   public:
    using iterator_category = std::input_iterator_tag;
    using value_type = const ElementType*;
    using difference_type = std::ptrdiff_t;
    using pointer = const ElementType**;
    using reference = const ElementType*&;

    const_iterator(const typename Map::const_iterator& it) : it_(it) {}
    const_iterator(const const_iterator& ids_it) : it_(ids_it.it_) {}

    const_iterator& operator++() {
      ++it_;
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator tmp(it_++);
      return tmp;
    }

    bool operator==(const const_iterator& rhs) const {
      return it_ == rhs.it_;
    }

    bool operator!=(const const_iterator& rhs) const {
      return it_ != rhs.it_;
    }

    const ElementType* operator*() const {
      return it_->second.get();
    }

    const ElementType* operator->() const {
      return it_->second.get();
    }

   private:
    typename Map::const_iterator it_;
  };

  BaseSetOperators() {
    // Ensure |T| is convertible to us, so we can safely downcast when calling
    // methods that must exist in |T|.
    static_assert(std::is_convertible<T*, BaseSetOperators<T>*>::value,
                  "U ptr must implicitly convert to T ptr");
  }

  BaseSetOperators(BaseSetOperators<T>&& other) = default;
  BaseSetOperators<T>& operator=(BaseSetOperators<T>&& rhs) = default;

  ~BaseSetOperators() {}

  bool operator==(const BaseSetOperators<T>& rhs) const { return Equal(rhs); }
  bool operator!=(const BaseSetOperators<T>& rhs) const { return !Equal(rhs); }

  T Clone() const {
    T result;
    for (const auto* item : *this)
      result.insert(item->Clone());
    return result;
  }

  bool Equal(const BaseSetOperators<T>& rhs) const {
    const_iterator it = begin();
    const_iterator rhs_it = rhs.begin();
    const_iterator it_end = end();
    const_iterator rhs_it_end = rhs.end();

    while (it != it_end && rhs_it != rhs_it_end) {
      if (it->id() > rhs_it->id())
        return false;
      else if (it->id() < rhs_it->id())
        return false;
      else if (!it->Equal(*rhs_it))
        return false;
      ++it;
      ++rhs_it;
    }
    return it == it_end && rhs_it == rhs_it_end;
  }

  bool Contains(const BaseSetOperators<T>& rhs) const {
    const_iterator it1 = begin();
    const_iterator it2 = rhs.begin();
    const_iterator end1 = end();
    const_iterator end2 = rhs.end();

    while (it1 != end1 && it2 != end2) {
      if (it1->id() > it2->id()) {
        return false;
      } else if (it1->id() < it2->id()) {
        ++it1;
      } else {
          if (!it1->Contains(*it2))
            return false;
        ++it1;
        ++it2;
      }
    }

    return it2 == end2;
  }

  static void Difference(const BaseSetOperators<T>& set1,
                         const BaseSetOperators<T>& set2,
                         T* set3) {
    CHECK(set3);
    set3->clear();

    const_iterator it1 = set1.begin();
    const_iterator it2 = set2.begin();
    const const_iterator end1 = set1.end();
    const const_iterator end2 = set2.end();

    while (it1 != end1 && it2 != end2) {
      if (it1->id() < it2->id()) {
        set3->insert(it1->Clone());
        ++it1;
      } else if (it1->id() > it2->id()) {
        ++it2;
      } else {
        std::unique_ptr<ElementType> p = it1->Diff(*it2);
        if (p)
          set3->insert(std::move(p));
        ++it1;
        ++it2;
      }
    }

    while (it1 != end1) {
      set3->insert(it1->Clone());
      ++it1;
    }
  }

  static void Intersection(const BaseSetOperators<T>& set1,
                           const BaseSetOperators<T>& set2,
                           T* set3) {
    DCHECK(set3);
    set3->clear();

    const_iterator it1 = set1.begin();
    const_iterator it2 = set2.begin();
    const const_iterator end1 = set1.end();
    const const_iterator end2 = set2.end();

    while (it1 != end1 && it2 != end2) {
      if (it1->id() < it2->id()) {
        ++it1;
      } else if (it1->id() > it2->id()) {
        ++it2;
      } else {
        std::unique_ptr<ElementType> p = it1->Intersect(*it2);
        if (p)
          set3->insert(std::move(p));
        ++it1;
        ++it2;
      }
    }
  }

  static void Union(const BaseSetOperators<T>& set1,
                    const BaseSetOperators<T>& set2,
                    T* set3) {
    DCHECK(set3);
    set3->clear();

    const_iterator it1 = set1.begin();
    const_iterator it2 = set2.begin();
    const const_iterator end1 = set1.end();
    const const_iterator end2 = set2.end();

    while (true) {
      if (it1 == end1) {
        while (it2 != end2) {
          set3->insert(it2->Clone());
          ++it2;
        }
        break;
      }
      if (it2 == end2) {
        while (it1 != end1) {
          set3->insert(it1->Clone());
          ++it1;
        }
        break;
      }
      if (it1->id() < it2->id()) {
        set3->insert(it1->Clone());
        ++it1;
      } else if (it1->id() > it2->id()) {
        set3->insert(it2->Clone());
        ++it2;
      } else {
        set3->insert(it1->Union(*it2));
        ++it1;
        ++it2;
      }
    }
  }

  const_iterator begin() const { return const_iterator(map_.begin()); }

  const_iterator end() const { return map_.end(); }

  const_iterator find(ElementIDType id) const { return map_.find(id); }

  size_t count(ElementIDType id) const { return map_.count(id); }

  bool empty() const { return map_.empty(); }

  size_t erase(ElementIDType id) { return map_.erase(id); }

  void insert(std::unique_ptr<ElementType> item) {
    ElementIDType id = item->id();
    map_[id] = std::move(item);
  }

  size_t size() const { return map_.size(); }

  const Map& map() const {
    return map_;
  }

  void clear() {
    map_.clear();
  }

 private:
  Map map_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_BASE_SET_OPERATORS_H_
