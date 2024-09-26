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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MATCH_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MATCH_REQUEST_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/rule_set.h"

namespace blink {

class ContainerNode;

// Encapsulates the context for matching against a group of style sheets
// by ElementRuleCollector. Carries the RuleSet, scope (a ContainerNode) and
// CSSStyleSheet.
//
// We allow up to 32 style sheets in a group. More than one allows us to
// amortize checks on the element between style sheets (e.g. fetching its
// parents, or lowercasing attributes), but having an arbitrary number of them
// (ie., using a Vector or HeapVector) would require us to either make the
// MatchRequest garbage-collected (with associated extra heap allocations),
// or lock down the rule sets using Persistent<>, which is also costly.
// This, we choose an in-between solution of grouping the stylesheet
// into bounded blocks; you can check with IsFull().
//
// All style sheets have an index, which are assumed to be consecutive.
class CORE_EXPORT MatchRequest {
  STACK_ALLOCATED();

 public:
  explicit MatchRequest(const ContainerNode* scope = nullptr,
                        Element* vtt_originating_element = nullptr)
      : scope_(scope), vtt_originating_element_(vtt_originating_element) {}

  // Convenience form for a single stylesheet (or zero).
  explicit MatchRequest(RuleSet* rule_set,
                        const ContainerNode* scope = nullptr,
                        const CSSStyleSheet* css_sheet = nullptr,
                        unsigned style_sheet_index = 0,
                        Element* vtt_originating_element = nullptr)
      : style_sheet_first_index_(style_sheet_index),
        scope_(scope),
        vtt_originating_element_(vtt_originating_element) {
    if (rule_set) {
      AddRuleset(rule_set);
    }
  }

  const ContainerNode* Scope() const { return scope_; }
  Element* VTTOriginatingElement() const { return vtt_originating_element_; }

  void AddRuleset(RuleSet* rule_set) {
    DCHECK(!IsFull());

    // Now that we're about to read from the RuleSet, we're done adding more
    // rules to the set and we should make sure it's compacted.
    rule_set->CompactRulesIfNeeded();
    rule_sets_[num_rule_sets_] = rule_set;
    ++num_rule_sets_;
  }

  bool IsEmpty() const { return num_rule_sets_ == 0; }
  bool IsFull() const { return num_rule_sets_ == kRulesetsRoom; }

  // Use if the request was full and you matched everything in it,
  // but want to keep adding new elements. The difference between this
  // and creating a new MatchRequest, is that the style sheet index
  // will keep incrementing.
  void ClearAfterMatching() {
    style_sheet_first_index_ += num_rule_sets_;
    num_rule_sets_ = 0;
  }

  // Used for returning from RuleSetIterator; not actually stored.
  struct RuleSetWithIndex {
    STACK_ALLOCATED();

   public:
    const RuleSet* rule_set;
    unsigned style_sheet_index;
  };

  // An iterator over all the rule sets, intended for use in range-based
  // for loops (use AllRuleSets()). The index is automatically generated
  // based on style_sheet_first_index_.
  class RuleSetIterator {
    STACK_ALLOCATED();

   public:
    RuleSetIterator(const MatchRequest* match_request, unsigned index)
        : match_request_(*match_request), index_(index) {}

    RuleSetWithIndex operator*() const {
      return {match_request_.rule_sets_[index_],
              index_ + match_request_.style_sheet_first_index_};
    }

    RuleSetIterator& operator++() {
      ++index_;
      return *this;
    }

    bool operator==(const RuleSetIterator& other) const {
      DCHECK_EQ(&match_request_, &other.match_request_);
      return index_ == other.index_;
    }

    bool operator!=(const RuleSetIterator& other) const {
      DCHECK_EQ(&match_request_, &other.match_request_);
      return index_ != other.index_;
    }

   private:
    const MatchRequest& match_request_;
    unsigned index_;
  };

  // A proxy object to allow AllRuleSets() to be iterable in a range-based
  // for loop (ie., provide begin() and end() member functions).
  class RuleSetIteratorProxy {
    STACK_ALLOCATED();

   public:
    explicit RuleSetIteratorProxy(const MatchRequest* match_request)
        : match_request_(*match_request) {}

    RuleSetIterator begin() const { return {&match_request_, 0}; }
    RuleSetIterator end() const {
      return {&match_request_, match_request_.num_rule_sets_};
    }

   private:
    const MatchRequest& match_request_;
  };
  RuleSetIteratorProxy AllRuleSets() const {
    return RuleSetIteratorProxy{this};
  }

 private:
  friend class RuleSetIterator;

  static constexpr unsigned kRulesetsRoom = 32;
  const RuleSet* rule_sets_[kRulesetsRoom];
  unsigned num_rule_sets_ = 0;
  unsigned style_sheet_first_index_ = 0;

  const ContainerNode* scope_;
  // For WebVTT STYLE blocks, this is set to the featureless-like Element
  // described by the spec:
  // https://w3c.github.io/webvtt/#obtaining-css-boxes
  Element* vtt_originating_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_MATCH_REQUEST_H_
