/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MATCH_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MATCH_RESULT_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CSSPropertyValueSet;

struct CORE_EXPORT MatchedProperties {
  DISALLOW_NEW();

 public:
  MatchedProperties();
  ~MatchedProperties();

  void Trace(blink::Visitor*);

  Member<CSSPropertyValueSet> properties;

  struct Data {
    unsigned link_match_type : 2;
    unsigned valid_property_filter : 2;
    // This is approximately equivalent to the 'shadow-including tree order'.
    // It can be used to evaluate the 'Shadow Tree' criteria. Note that the
    // number stored here is 'local' to each origin (user, author), and is
    // not used at all for the UA origin. Hence, it is not possible to compare
    // tree_orders from two different origins.
    //
    // Note also that the tree_order will start at ~0u and then decrease.
    // This is because we currently store the matched properties in reverse
    // order.
    //
    // https://drafts.csswg.org/css-scoping/#shadow-cascading
    uint16_t tree_order;
  };
  Data types_;
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::MatchedProperties)

namespace blink {

using MatchedPropertiesVector = HeapVector<MatchedProperties, 64>;

// MatchedPropertiesRange is used to represent a subset of the matched
// properties from a given origin, for instance UA rules, author rules, or a
// shadow tree scope.  This is needed because rules from different origins are
// applied in the opposite order for !important rules, yet in the same order as
// for normal rules within the same origin.

class MatchedPropertiesRange {
 public:
  MatchedPropertiesRange(MatchedPropertiesVector::const_iterator begin,
                         MatchedPropertiesVector::const_iterator end)
      : begin_(begin), end_(end) {}

  MatchedPropertiesVector::const_iterator begin() const { return begin_; }
  MatchedPropertiesVector::const_iterator end() const { return end_; }

  bool IsEmpty() const { return begin() == end(); }

 private:
  MatchedPropertiesVector::const_iterator begin_;
  MatchedPropertiesVector::const_iterator end_;
};

class CORE_EXPORT MatchResult {
  STACK_ALLOCATED();

 public:
  MatchResult() = default;

  void AddMatchedProperties(
      const CSSPropertyValueSet* properties,
      unsigned link_match_type = CSSSelector::kMatchAll,
      ValidPropertyFilter = ValidPropertyFilter::kNoFilter);
  bool HasMatchedProperties() const { return matched_properties_.size(); }

  void FinishAddingUARules();
  void FinishAddingUserRules();
  void FinishAddingAuthorRulesForTreeScope();

  void SetIsCacheable(bool cacheable) { is_cacheable_ = cacheable; }
  bool IsCacheable() const { return is_cacheable_; }

  MatchedPropertiesRange AllRules() const {
    return MatchedPropertiesRange(matched_properties_.begin(),
                                  matched_properties_.end());
  }
  MatchedPropertiesRange UaRules() const {
    MatchedPropertiesVector::const_iterator begin = matched_properties_.begin();
    MatchedPropertiesVector::const_iterator end =
        matched_properties_.begin() + ua_range_end_;
    return MatchedPropertiesRange(begin, end);
  }
  MatchedPropertiesRange UserRules() const {
    MatchedPropertiesVector::const_iterator begin =
        matched_properties_.begin() + ua_range_end_;
    MatchedPropertiesVector::const_iterator end =
        matched_properties_.begin() + (user_range_ends_.IsEmpty()
                                           ? ua_range_end_
                                           : user_range_ends_.back());
    return MatchedPropertiesRange(begin, end);
  }
  MatchedPropertiesRange AuthorRules() const {
    MatchedPropertiesVector::const_iterator begin =
        matched_properties_.begin() + (user_range_ends_.IsEmpty()
                                           ? ua_range_end_
                                           : user_range_ends_.back());
    MatchedPropertiesVector::const_iterator end = matched_properties_.end();
    return MatchedPropertiesRange(begin, end);
  }

  const MatchedPropertiesVector& GetMatchedProperties() const {
    return matched_properties_;
  }

 private:
  friend class ImportantUserRanges;
  friend class ImportantUserRangeIterator;
  friend class ImportantAuthorRanges;
  friend class ImportantAuthorRangeIterator;

  MatchedPropertiesVector matched_properties_;
  Vector<unsigned, 16> user_range_ends_;
  Vector<unsigned, 16> author_range_ends_;
  unsigned ua_range_end_ = 0;
  bool is_cacheable_ = true;
  uint16_t current_tree_order_ = 0;
  DISALLOW_COPY_AND_ASSIGN(MatchResult);
};

class ImportantUserRangeIterator {
  STACK_ALLOCATED();

 public:
  ImportantUserRangeIterator(const MatchResult& result, int end_index)
      : result_(result), end_index_(end_index) {}

  MatchedPropertiesRange operator*() const {
    unsigned range_end = result_.user_range_ends_[end_index_];
    unsigned range_begin = end_index_
                               ? result_.user_range_ends_[end_index_ - 1]
                               : result_.ua_range_end_;
    return MatchedPropertiesRange(
        result_.GetMatchedProperties().begin() + range_begin,
        result_.GetMatchedProperties().begin() + range_end);
  }

  ImportantUserRangeIterator& operator++() {
    --end_index_;
    return *this;
  }

  bool operator==(const ImportantUserRangeIterator& other) const {
    return end_index_ == other.end_index_ && &result_ == &other.result_;
  }
  bool operator!=(const ImportantUserRangeIterator& other) const {
    return !(*this == other);
  }

 private:
  const MatchResult& result_;
  unsigned end_index_;
};

class ImportantUserRanges {
  STACK_ALLOCATED();

 public:
  explicit ImportantUserRanges(const MatchResult& result) : result_(result) {}

  ImportantUserRangeIterator begin() const {
    return ImportantUserRangeIterator(result_,
                                        result_.user_range_ends_.size() - 1);
  }
  ImportantUserRangeIterator end() const {
    return ImportantUserRangeIterator(result_, -1);
  }

 private:
  const MatchResult& result_;
};

class ImportantAuthorRangeIterator {
  STACK_ALLOCATED();

 public:
  ImportantAuthorRangeIterator(const MatchResult& result, int end_index)
      : result_(result), end_index_(end_index) {}

  MatchedPropertiesRange operator*() const {
    unsigned range_end = result_.author_range_ends_[end_index_];
    unsigned range_begin = end_index_
                               ? result_.author_range_ends_[end_index_ - 1]
                               : (result_.user_range_ends_.IsEmpty()
                                      ? result_.ua_range_end_
                                      : result_.user_range_ends_.back());
    return MatchedPropertiesRange(
        result_.GetMatchedProperties().begin() + range_begin,
        result_.GetMatchedProperties().begin() + range_end);
  }

  ImportantAuthorRangeIterator& operator++() {
    --end_index_;
    return *this;
  }

  bool operator==(const ImportantAuthorRangeIterator& other) const {
    return end_index_ == other.end_index_ && &result_ == &other.result_;
  }
  bool operator!=(const ImportantAuthorRangeIterator& other) const {
    return !(*this == other);
  }

 private:
  const MatchResult& result_;
  unsigned end_index_;
};

class ImportantAuthorRanges {
  STACK_ALLOCATED();

 public:
  explicit ImportantAuthorRanges(const MatchResult& result) : result_(result) {}

  ImportantAuthorRangeIterator begin() const {
    return ImportantAuthorRangeIterator(result_,
                                        result_.author_range_ends_.size() - 1);
  }
  ImportantAuthorRangeIterator end() const {
    return ImportantAuthorRangeIterator(result_, -1);
  }

 private:
  const MatchResult& result_;
};

inline bool operator==(const MatchedProperties& a, const MatchedProperties& b) {
  return a.properties == b.properties &&
         a.types_.link_match_type == b.types_.link_match_type;
}

inline bool operator!=(const MatchedProperties& a, const MatchedProperties& b) {
  return !(a == b);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MATCH_RESULT_H_
