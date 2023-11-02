// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_ROOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_ROOT_H_

#include "base/check_op.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/core/style/toggle_group.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

enum class ToggleOverflow : uint8_t { kCycle = 0, kCycleOn = 1, kSticky = 2 };

// This ToggleRoot class is used to represent both a value of the toggle-root
// property, which the specification also calls a toggle specifier, and to
// represent (via its derived class Toggle) what the specification calls a
// toggle, which is state associated with the element.  (The former would be
// const if WTF::Vector supported it, whereas the latter should not be.)
class ToggleRoot {
  DISALLOW_NEW();

 public:
  class States {
   public:
    using IntegerType = uint32_t;
    using NamesType = WTF::Vector<AtomicString>;

    enum class Type : std::size_t { Integer = 0, Names = 1 };

    explicit States(IntegerType integer) : value_(integer) {}
    explicit States(const NamesType& names) : value_(names) {}

    bool operator==(const States& other) const {
      return value_ == other.value_;
    }
    bool operator!=(const States& other) const {
      return value_ != other.value_;
    }

    Type GetType() const { return Type(value_.index()); }

    bool IsInteger() const { return GetType() == Type::Integer; }
    bool IsNames() const { return GetType() == Type::Names; }

    IntegerType AsInteger() const {
      return absl::get<std::size_t(Type::Integer)>(value_);
    }
    const NamesType& AsNames() const {
      return absl::get<std::size_t(Type::Names)>(value_);
    }

   private:
    absl::variant<IntegerType, NamesType> value_;
  };

  class State {
   public:
    using IntegerType = uint32_t;
    using NameType = AtomicString;

    enum class Type : std::size_t { Integer = 0, Name = 1 };

    explicit State(IntegerType integer) : value_(integer) {}
    explicit State(const NameType& names) : value_(names) {}

    bool operator==(const State& other) const { return value_ == other.value_; }
    bool operator!=(const State& other) const { return value_ != other.value_; }

    Type GetType() const { return Type(value_.index()); }

    bool IsInteger() const { return GetType() == Type::Integer; }
    bool IsName() const { return GetType() == Type::Name; }

    IntegerType AsInteger() const {
      return absl::get<std::size_t(Type::Integer)>(value_);
    }
    const NameType& AsName() const {
      return absl::get<std::size_t(Type::Name)>(value_);
    }

    String ToString() const;

   private:
    absl::variant<IntegerType, NameType> value_;
  };

  ToggleRoot(const AtomicString& name,
             States states,
             State initial_state,
             ToggleOverflow overflow,
             bool is_group,
             ToggleScope scope)
      : name_(name),
        states_(states),
        value_(initial_state),
        overflow_(overflow),
        is_group_(is_group),
        scope_(scope) {
    DCHECK_EQ(scope_, scope) << "sufficient field width";
    DCHECK_EQ(overflow_, overflow) << "sufficient field width";
  }

 protected:
  ToggleRoot()
      : states_(1),
        value_(0),
        overflow_(ToggleOverflow::kCycle),
        is_group_(false),
        scope_(ToggleScope::kWide) {}

 public:
  ToggleRoot(const ToggleRoot&) = default;
  ~ToggleRoot() = default;

  bool operator==(const ToggleRoot& other) const {
    return name_ == other.name_ && states_ == other.states_ &&
           value_ == other.value_ && overflow_ == other.overflow_ &&
           is_group_ == other.is_group_ && scope_ == other.scope_;
  }
  bool operator!=(const ToggleRoot& other) const { return !(*this == other); }

  const AtomicString& Name() const { return name_; }
  const States& StateSet() const { return states_; }
  const State& InitialState() const { return value_; }
  ToggleOverflow Overflow() const { return overflow_; }
  bool IsGroup() const { return is_group_; }
  ToggleScope Scope() const { return scope_; }

 protected:
  AtomicString name_;
  States states_;
  State value_;
  ToggleOverflow overflow_;
  bool is_group_;
  ToggleScope scope_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_ROOT_H_
