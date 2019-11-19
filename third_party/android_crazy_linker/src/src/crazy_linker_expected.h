// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_OUTCOME_H
#define CRAZY_LINKER_OUTCOME_H

#include "crazy_linker_debug.h"
#include "crazy_linker_error.h"

#include <utility>

namespace crazy {

// Handy template for an object that can be either a value of type T, or hold
// a non-owning pointer to a crazy::Error instance. The reason for this design
// is to keep each Expected<T> instance small, since each Error instance is
// pretty large (over 512 bytes), and generally allocated on the stack by the
// caller. Usage examples:
//
//    Expected<int> getFoo() { return 42; }
//    Expected<int> getBar(Error* error)  { return Expected<int>(error); }
//    Expected<int> getBar2(Error* error) { return error; }  // equivalent!
template <class T>
struct Expected {
  // No default constructor.
  Expected() = delete;

  // Value constructor.
  constexpr Expected(const T& value) : has_value_(true), value_(value) {}
  Expected(T&& value) : has_value_(true), value_(std::move(value)) {}

  // Null-constructor, only valid if T can be constructed from nullptr.
  constexpr Expected(nullptr_t) : has_value_(true), value_(nullptr) {}

  // Error constructor.
  Expected(Error* error) : has_value_(false), error_(error) {}

  // Move constructor.
  Expected(Expected&& other) noexcept : has_value_(other.has_value_) {
    if (has_value_) {
      value_ = std::move(other.value_);
    } else {
      error_ = other.error_;
    }
  }

  // Move assigment.
  Expected& operator=(Expected&& other) noexcept {
    if (this != &other) {
      this->~Expected();
      *this = std::move(other);
    }
    return *this;
  }

  // Destructor.
  ~Expected() {
    if (has_value_) {
      value_.~T();
    }
  }

  // Bool operator, to write: if (expected) { ... use value. }
  constexpr explicit operator bool() const { return has_value_; }

  // Return reference to value. Assert if error.
  constexpr const T& operator*() const { return value(); }
  T& operator*() { return value(); }

  constexpr const T& value() const& {
    return ASSERT(has_value_, "No value in Expected<> instance!"), value_;
  }

  T& value() & {
    return ASSERT(has_value_, "No value in Expected<> instance!"), value_;
  }

  T&& value() && {
    return ASSERT(has_value_, "No value in Expected<> instance!"),
           std::move(value_);
  }

  // Return reference to value, or a default value if it is an error.
  constexpr const T& value_or(const T& default_value) const {
    return has_value_ ? value_ : default_value;
  }

  // Return reference to error. Assert if value.
  constexpr const Error* error() const {
    ASSERT(!has_value_, "No error in Expected<> instance!");
    return error_;
  }

  constexpr bool has_value() const { return has_value_; }
  constexpr bool has_error() const { return !has_value_; }

 private:
  bool has_value_;
  union {
    T value_;
    Error* error_;
  };
};

}  // namespace crazy

#endif  // CRAZY_LINKER_OUTCOME_H
