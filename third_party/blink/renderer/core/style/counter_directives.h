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

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class CounterDirectives {
  DISALLOW_NEW();

 public:
  CounterDirectives()
      : is_reset_set_(false),
        is_increment_set_(false),
        reset_value_(0),
        increment_value_(0) {}

  // FIXME: The code duplication here could possibly be replaced by using two
  // maps, or by using a container that held two generic Directive objects.

  bool IsReset() const { return is_reset_set_; }
  int ResetValue() const { return reset_value_; }
  void SetResetValue(int value) {
    reset_value_ = value;
    is_reset_set_ = true;
  }
  void ClearReset() {
    reset_value_ = 0;
    is_reset_set_ = false;
  }
  void InheritReset(const CounterDirectives& parent) {
    reset_value_ = parent.reset_value_;
    is_reset_set_ = parent.is_reset_set_;
  }

  bool IsIncrement() const { return is_increment_set_; }
  int IncrementValue() const { return increment_value_; }
  void AddIncrementValue(int value) {
    increment_value_ = clampTo<int>((double)increment_value_ + value);
    is_increment_set_ = true;
  }
  void ClearIncrement() {
    increment_value_ = 0;
    is_increment_set_ = false;
  }
  void InheritIncrement(const CounterDirectives& parent) {
    increment_value_ = parent.increment_value_;
    is_increment_set_ = parent.is_increment_set_;
  }

  bool IsDefined() const { return IsReset() || IsIncrement(); }

  int CombinedValue() const {
    DCHECK(is_reset_set_ || !reset_value_);
    DCHECK(is_increment_set_ || !increment_value_);
    // According to the spec, if an increment would overflow or underflow the
    // counter, we are allowed to ignore the increment.
    // https://drafts.csswg.org/css-lists-3/#valdef-counter-reset-custom-ident-integer
    return base::CheckAdd(reset_value_, increment_value_)
        .ValueOrDefault(reset_value_);
  }

 private:
  bool is_reset_set_;
  bool is_increment_set_;
  int reset_value_;
  int increment_value_;
};

bool operator==(const CounterDirectives&, const CounterDirectives&);
inline bool operator!=(const CounterDirectives& a, const CounterDirectives& b) {
  return !(a == b);
}

// Not to be deleted through a pointer to HashMap.
class CounterDirectiveMap : public HashMap<AtomicString, CounterDirectives> {
 public:
  std::unique_ptr<CounterDirectiveMap> Clone() const {
    return base::WrapUnique(new CounterDirectiveMap(*this));
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COUNTER_DIRECTIVES_H_
