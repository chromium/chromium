/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COUNTER_DIRECTIVES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COUNTER_DIRECTIVES_H_

#include <memory>
#include <optional>

#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/clamped_math.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

// CounterDirectives stores the `counter-reset`, `counter-increment`, and
// `counter-set` properties for a single counter identifier. An object of this
// class represents all the counter-related directives specified on an element
// for one specific counter.
class CounterDirectives {
  DISALLOW_NEW();

 public:
  CounterDirectives() = default;

  // FIXME: The code duplication here could possibly be replaced by using two
  // maps, or by using a container that held two generic Directive objects.

  // https://drafts.csswg.org/css-lists-3/#counter-reset

  // Returns true when `counter-reset` either holds an <integer> value or it is
  // reversed.
  bool IsReset() const {
    return reset_value_.has_value() || is_reset_reversed_;
  }
  // Returns an optional <integer> value of `counter-reset`. The return value is
  // `nullopt` if the reversed function is specified and the <integer> is
  // omitted(e.g., `counter-reset: reversed(list-item)`).
  std::optional<int> ResetValue() const { return reset_value_; }
  void SetResetValue(int value) { reset_value_ = value; }
  bool IsResetReversed() const { return is_reset_reversed_; }
  void SetIsResetReversed() { is_reset_reversed_ = true; }
  void ClearReset() {
    reset_value_.reset();
    is_reset_reversed_ = false;
  }
  void InheritReset(const CounterDirectives& parent) {
    reset_value_ = parent.reset_value_;
    is_reset_reversed_ = parent.is_reset_reversed_;
  }

  // https://drafts.csswg.org/css-lists-3/#increment-set

  bool HasIncrement() const { return increment_value_.has_value(); }
  // Returns an <integer> value of `counter-increment`. If |HasIncrement()| is
  // false, this call will crash. Otherwise, no crash happens even if the
  // <integer> is omitted(e.g., `counter-increment: foo-bar`) because it has
  // a default value.
  int IncrementValue() const { return *increment_value_; }
  void AddIncrementValue(int value) {
    increment_value_ = base::ClampAdd(increment_value_.value_or(0), value);
  }
  void ClearIncrement() { increment_value_.reset(); }
  void InheritIncrement(const CounterDirectives& parent) {
    increment_value_ = parent.increment_value_;
  }

  bool HasSet() const { return set_value_.has_value(); }
  // Returns an <integer> value of `counter-set`. If |HasSet()| is false, this
  // call will crash. Otherwise, no crash happens even if the <integer> is
  // omitted(e.g., `counter-set: foo-bar`) because it has a default value.
  int SetValue() const { return *set_value_; }
  void SetSetValue(int value) { set_value_ = value; }
  void ClearSet() { set_value_.reset(); }
  void InheritSet(const CounterDirectives& parent) {
    set_value_ = parent.set_value_;
  }

  bool IsDefined() const { return IsReset() || HasIncrement() || HasSet(); }

  int CombinedValue() const {
    // If there is a counter-set, it overrides over values.
    // https://drafts.csswg.org/css-lists-3/#auto-numbering
    if (HasSet()) {
      return SetValue();
    }

    // According to the spec, if an increment would overflow or underflow the
    // counter, we are allowed to ignore the increment.
    // https://drafts.csswg.org/css-lists-3/#valdef-counter-reset-custom-ident-integer
    return base::CheckAdd(reset_value_.value_or(0),
                          increment_value_.value_or(0))
        .ValueOrDefault(reset_value_.value_or(0));
  }

  bool operator==(const CounterDirectives&) const = default;

 private:
  std::optional<int> reset_value_;
  std::optional<int> increment_value_;
  std::optional<int> set_value_;
  bool is_reset_reversed_ = false;
};

// Not to be deleted through a pointer to HashMap.
class CounterDirectiveMap : public HashMap<AtomicString, CounterDirectives> {
 public:
  std::unique_ptr<CounterDirectiveMap> Clone() const {
    return base::WrapUnique(new CounterDirectiveMap(*this));
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COUNTER_DIRECTIVES_H_
