// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Utilities for testing exception-safety

#ifndef ABSL_BASE_INTERNAL_EXCEPTION_SAFETY_TESTING_H_
#define ABSL_BASE_INTERNAL_EXCEPTION_SAFETY_TESTING_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iosfwd>
#include <string>
#include <tuple>
#include <unordered_map>

#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/base/internal/pretty_function.h"
#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/types/optional.h"

namespace testing {

enum class TypeSpec;
enum class AllocSpec;

constexpr TypeSpec operator|(TypeSpec a, TypeSpec b) {
  using T = absl::underlying_type_t<TypeSpec>;
  return static_cast<TypeSpec>(static_cast<T>(a) | static_cast<T>(b));
}

constexpr TypeSpec operator&(TypeSpec a, TypeSpec b) {
  using T = absl::underlying_type_t<TypeSpec>;
  return static_cast<TypeSpec>(static_cast<T>(a) & static_cast<T>(b));
}

constexpr AllocSpec operator|(AllocSpec a, AllocSpec b) {
  using T = absl::underlying_type_t<AllocSpec>;
  return static_cast<AllocSpec>(static_cast<T>(a) | static_cast<T>(b));
}

constexpr AllocSpec operator&(AllocSpec a, AllocSpec b) {
  using T = absl::underlying_type_t<AllocSpec>;
  return static_cast<AllocSpec>(static_cast<T>(a) & static_cast<T>(b));
}

namespace exceptions_internal {

std::string GetSpecString(TypeSpec);
std::string GetSpecString(AllocSpec);

struct NoThrowTag {};
struct StrongGuaranteeTagType {};

// A simple exception class.  We throw this so that test code can catch
// exceptions specifically thrown by ThrowingValue.
class TestException {
 public:
  explicit TestException(absl::string_view msg) : msg_(msg) {}
  virtual ~TestException() {}
  virtual const char* what() const noexcept { return msg_.c_str(); }

 private:
  std::string msg_;
};

// TestBadAllocException exists because allocation functions must throw an
// exception which can be caught by a handler of std::bad_alloc.  We use a child
// class of std::bad_alloc so we can customise the error message, and also
// derive from TestException so we don't accidentally end up catching an actual
// bad_alloc exception in TestExceptionSafety.
class TestBadAllocException : public std::bad_alloc, public TestException {
 public:
  explicit TestBadAllocException(absl::string_view msg) : TestException(msg) {}
  using TestException::what;
};

extern int countdown;

// Allows the countdown variable to be set manually (defaulting to the initial
// value of 0)
inline void SetCountdown(int i = 0) { countdown = i; }
// Sets the countdown to the terminal value -1
inline void UnsetCountdown() { SetCountdown(-1); }

void MaybeThrow(absl::string_view msg, bool throw_bad_alloc = false);

testing::AssertionResult FailureMessage(const TestException& e,
                                        int countdown) noexcept;

struct TrackedAddress {
  bool is_alive;
  std::string description;
};

// Inspects the constructions and destructions of anything inheriting from
// TrackedObject. This allows us to safely "leak" TrackedObjects, as
// ConstructorTracker will destroy everything left over in its destructor.
class ConstructorTracker {
 public:
  explicit ConstructorTracker(int count) : countdown_(count) {
    assert(current_tracker_instance_ == nullptr);
    current_tracker_instance_ = this;
  }

  ~ConstructorTracker() {
    assert(current_tracker_instance_ == this);
    current_tracker_instance_ = nullptr;

    for (auto& it : address_map_) {
      void* address = it.first;
      TrackedAddress& tracked_address = it.second;
      if (tracked_address.is_alive) {
        ADD_FAILURE() << "Object at address " << address
                      << " with countdown of " << countdown_
                      << " was not destroyed [" << tracked_address.description
                      << "]";
      }
    }
  }

  static void ObjectConstructed(void* address, std::string description) {
    if (!CurrentlyTracking()) return;

    TrackedAddress& tracked_address =
        current_tracker_instance_->address_map_[address];
    if (tracked_address.is_alive) {
      ADD_FAILURE() << "Object at address " << address << " with countdown of "
                    << current_tracker_instance_->countdown_
                    << " was re-constructed. Previously: ["
                    << tracked_address.description << "] Now: [" << description
                    << "]";
    }
    tracked_address = {true, std::move(description)};
  }

  static void ObjectDestructed(void* address) {
    if (!CurrentlyTracking()) return;

    auto it = current_tracker_instance_->address_map_.find(address);
    // Not tracked. Ignore.
    if (it == current_tracker_instance_->address_map_.end()) return;

    TrackedAddress& tracked_address = it->second;
    if (!tracked_address.is_alive) {
      ADD_FAILURE() << "Object at address " << address << " with countdown of "
                    << current_tracker_instance_->countdown_
                    << " was re-destroyed or created prior to construction "
                    << "tracking [" << tracked_address.description << "]";
    }
    tracked_address.is_alive = false;
  }

 private:
  static bool CurrentlyTracking() {
    return current_tracker_instance_ != nullptr;
  }

  std::unordered_map<void*, TrackedAddress> address_map_;
  int countdown_;

  static ConstructorTracker* current_tracker_instance_;
};

class TrackedObject {
 public:
  TrackedObject(const TrackedObject&) = delete;
  TrackedObject(TrackedObject&&) = delete;

 protected:
  explicit TrackedObject(std::string description) {
    ConstructorTracker::ObjectConstructed(this, std::move(description));
  }

  ~TrackedObject() noexcept { ConstructorTracker::ObjectDestructed(this); }
};

template <typename Factory, typename Operation, typename Contract>
absl::optional<testing::AssertionResult> TestSingleContractAtCountdownImpl(
    const Factory& factory, const Operation& operation, int count,
    const Contract& contract) {
  auto t_ptr = factory();
  absl::optional<testing::AssertionResult> current_res;
  SetCountdown(count);
  try {
    operation(t_ptr.get());
  } catch (const exceptions_internal::TestException& e) {
    current_res.emplace(contract(t_ptr.get()));
    if (!current_res.value()) {
      *current_res << e.what() << " failed contract check";
    }
  }
  UnsetCountdown();
  return current_res;
}

template <typename Factory, typename Operation>
absl::optional<testing::AssertionResult> TestSingleContractAtCountdownImpl(
    const Factory& factory, const Operation& operation, int count,
    StrongGuaranteeTagType) {
  using TPtr = typename decltype(factory())::pointer;
  auto t_is_strong = [&](TPtr t) { return *t == *factory(); };
  return TestSingleContractAtCountdownImpl(factory, operation, count,
                                           t_is_strong);
}

template <typename Factory, typename Operation, typename Contract>
int TestSingleContractAtCountdown(
    const Factory& factory, const Operation& operation, int count,
    const Contract& contract,
    absl::optional<testing::AssertionResult>* reduced_res) {
  // If reduced_res is empty, it means the current call to
  // TestSingleContractAtCountdown(...) is the first test being run so we do
  // want to run it. Alternatively, if it's not empty (meaning a previous test
  // has run) we want to check if it passed. If the previous test did pass, we
  // want to contine running tests so we do want to run the current one. If it
  // failed, we want to short circuit so as not to overwrite the AssertionResult
  // output. If that's the case, we do not run the current test and instead we
  // simply return.
  if (!reduced_res->has_value() || reduced_res->value()) {
    *reduced_res =
        TestSingleContractAtCountdownImpl(factory, operation, count, contract);
  }
  return 0;
}

template <typename Factory, typename Operation, typename... Contracts>
inline absl::optional<testing::AssertionResult> TestAllContractsAtCountdown(
    const Factory& factory, const Operation& operation, int count,
    const Contracts&... contracts) {
  absl::optional<testing::AssertionResult> reduced_res;

  // Run each checker, short circuiting after the first failure
  int dummy[] = {
      0, (TestSingleContractAtCountdown(factory, operation, count, contracts,
                                        &reduced_res))...};
  static_cast<void>(dummy);
  return reduced_res;
}

}  // namespace exceptions_internal

extern exceptions_internal::NoThrowTag nothrow_ctor;

extern exceptions_internal::StrongGuaranteeTagType strong_guarantee;

// A test class which is convertible to bool.  The conversion can be
// instrumented to throw at a controlled time.
class ThrowingBool {
 public:
  ThrowingBool(bool b) noexcept : b_(b) {}  // NOLINT(runtime/explicit)
  operator bool() const {                   // NOLINT
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return b_;
  }

 private:
  bool b_;
};

/*
 * Configuration enum for the ThrowingValue type that defines behavior for the
 * lifetime of the instance. Use testing::nothrow_ctor to prevent the integer
 * constructor from throwing.
 *
 * kEverythingThrows: Every operation can throw an exception
 * kNoThrowCopy: Copy construction and copy assignment will not throw
 * kNoThrowMove: Move construction and move assignment will not throw
 * kNoThrowNew: Overloaded operators new and new[] will not throw
 */
enum class TypeSpec {
  kEverythingThrows = 0,
  kNoThrowCopy = 1,
  kNoThrowMove = 1 << 1,
  kNoThrowNew = 1 << 2,
};

/*
 * A testing class instrumented to throw an exception at a controlled time.
 *
 * ThrowingValue implements a slightly relaxed version of the Regular concept --
 * that is it's a value type with the expected semantics.  It also implements
 * arithmetic operations.  It doesn't implement member and pointer operators
 * like operator-> or operator[].
 *
 * ThrowingValue can be instrumented to have certain operations be noexcept by
 * using compile-time bitfield template arguments.  That is, to make an
 * ThrowingValue which has noexcept move construction/assignment and noexcept
 * copy construction/assignment, use the following:
 *   ThrowingValue<testing::kNoThrowMove | testing::kNoThrowCopy> my_thrwr{val};
 */
template <TypeSpec Spec = TypeSpec::kEverythingThrows>
class ThrowingValue : private exceptions_internal::TrackedObject {
  static constexpr bool IsSpecified(TypeSpec spec) {
    return static_cast<bool>(Spec & spec);
  }

  static constexpr int kDefaultValue = 0;
  static constexpr int kBadValue = 938550620;

 public:
  ThrowingValue() : TrackedObject(GetInstanceString(kDefaultValue)) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    dummy_ = kDefaultValue;
  }

  ThrowingValue(const ThrowingValue& other) noexcept(
      IsSpecified(TypeSpec::kNoThrowCopy))
      : TrackedObject(GetInstanceString(other.dummy_)) {
    if (!IsSpecified(TypeSpec::kNoThrowCopy)) {
      exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    }
    dummy_ = other.dummy_;
  }

  ThrowingValue(ThrowingValue&& other) noexcept(
      IsSpecified(TypeSpec::kNoThrowMove))
      : TrackedObject(GetInstanceString(other.dummy_)) {
    if (!IsSpecified(TypeSpec::kNoThrowMove)) {
      exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    }
    dummy_ = other.dummy_;
  }

  explicit ThrowingValue(int i) : TrackedObject(GetInstanceString(i)) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    dummy_ = i;
  }

  ThrowingValue(int i, exceptions_internal::NoThrowTag) noexcept
      : TrackedObject(GetInstanceString(i)), dummy_(i) {}

  // absl expects nothrow destructors
  ~ThrowingValue() noexcept = default;

  ThrowingValue& operator=(const ThrowingValue& other) noexcept(
      IsSpecified(TypeSpec::kNoThrowCopy)) {
    dummy_ = kBadValue;
    if (!IsSpecified(TypeSpec::kNoThrowCopy)) {
      exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    }
    dummy_ = other.dummy_;
    return *this;
  }

  ThrowingValue& operator=(ThrowingValue&& other) noexcept(
      IsSpecified(TypeSpec::kNoThrowMove)) {
    dummy_ = kBadValue;
    if (!IsSpecified(TypeSpec::kNoThrowMove)) {
      exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    }
    dummy_ = other.dummy_;
    return *this;
  }

  // Arithmetic Operators
  ThrowingValue operator+(const ThrowingValue& other) const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return ThrowingValue(dummy_ + other.dummy_, nothrow_ctor);
  }

  ThrowingValue operator+() const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return ThrowingValue(dummy_, nothrow_ctor);
  }

  ThrowingValue operator-(const ThrowingValue& other) const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return ThrowingValue(dummy_ - other.dummy_, nothrow_ctor);
  }

  ThrowingValue operator-() const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return ThrowingValue(-dummy_, nothrow_ctor);
  }

  ThrowingValue& operator++() {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    ++dummy_;
    return *this;
  }

  ThrowingValue operator++(int) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    auto out = ThrowingValue(dummy_, nothrow_ctor);
    ++dummy_;
    return out;
  }

  ThrowingValue& operator--() {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    --dummy_;
    return *this;
  }

  ThrowingValue operator--(int) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    auto out = ThrowingValue(dummy_, nothrow_ctor);
    --dummy_;
    return out;
  }

  ThrowingValue operator*(const ThrowingValue& other) const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return ThrowingValue(dummy_ * other.dummy_, nothrow_ctor);
  }

  ThrowingValue operator/(const ThrowingValue& other) const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return ThrowingValue(dummy_ / other.dummy_, nothrow_ctor);
  }

  ThrowingValue operator%(const ThrowingValue& other) const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return ThrowingValue(dummy_ % other.dummy_, nothrow_ctor);
  }

  ThrowingValue operator<<(int shift) const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return ThrowingValue(dummy_ << shift, nothrow_ctor);
  }

  ThrowingValue operator>>(int shift) const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return ThrowingValue(dummy_ >> shift, nothrow_ctor);
  }

  // Comparison Operators
  // NOTE: We use `ThrowingBool` instead of `bool` because most STL
  // types/containers requires T to be convertible to bool.
  friend ThrowingBool operator==(const ThrowingValue& a,
                                 const ThrowingValue& b) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return a.dummy_ == b.dummy_;
  }
  friend ThrowingBool operator!=(const ThrowingValue& a,
                                 const ThrowingValue& b) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return a.dummy_ != b.dummy_;
  }
  friend ThrowingBool operator<(const ThrowingValue& a,
                                const ThrowingValue& b) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return a.dummy_ < b.dummy_;
  }
  friend ThrowingBool operator<=(const ThrowingValue& a,
                                 const ThrowingValue& b) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return a.dummy_ <= b.dummy_;
  }
  friend ThrowingBool operator>(const ThrowingValue& a,
                                const ThrowingValue& b) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return a.dummy_ > b.dummy_;
  }
  friend ThrowingBool operator>=(const ThrowingValue& a,
                                 const ThrowingValue& b) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return a.dummy_ >= b.dummy_;
  }

  // Logical Operators
  ThrowingBool operator!() const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return !dummy_;
  }

  ThrowingBool operator&&(const ThrowingValue& other) const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return dummy_ && other.dummy_;
  }

  ThrowingBool operator||(const ThrowingValue& other) const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return dummy_ || other.dummy_;
  }

  // Bitwise Logical Operators
  ThrowingValue operator~() const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return ThrowingValue(~dummy_, nothrow_ctor);
  }

  ThrowingValue operator&(const ThrowingValue& other) const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return ThrowingValue(dummy_ & other.dummy_, nothrow_ctor);
  }

  ThrowingValue operator|(const ThrowingValue& other) const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return ThrowingValue(dummy_ | other.dummy_, nothrow_ctor);
  }

  ThrowingValue operator^(const ThrowingValue& other) const {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return ThrowingValue(dummy_ ^ other.dummy_, nothrow_ctor);
  }

  // Compound Assignment operators
  ThrowingValue& operator+=(const ThrowingValue& other) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    dummy_ += other.dummy_;
    return *this;
  }

  ThrowingValue& operator-=(const ThrowingValue& other) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    dummy_ -= other.dummy_;
    return *this;
  }

  ThrowingValue& operator*=(const ThrowingValue& other) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    dummy_ *= other.dummy_;
    return *this;
  }

  ThrowingValue& operator/=(const ThrowingValue& other) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    dummy_ /= other.dummy_;
    return *this;
  }

  ThrowingValue& operator%=(const ThrowingValue& other) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    dummy_ %= other.dummy_;
    return *this;
  }

  ThrowingValue& operator&=(const ThrowingValue& other) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    dummy_ &= other.dummy_;
    return *this;
  }

  ThrowingValue& operator|=(const ThrowingValue& other) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    dummy_ |= other.dummy_;
    return *this;
  }

  ThrowingValue& operator^=(const ThrowingValue& other) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    dummy_ ^= other.dummy_;
    return *this;
  }

  ThrowingValue& operator<<=(int shift) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    dummy_ <<= shift;
    return *this;
  }

  ThrowingValue& operator>>=(int shift) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    dummy_ >>= shift;
    return *this;
  }

  // Pointer operators
  void operator&() const = delete;  // NOLINT(runtime/operator)

  // Stream operators
  friend std::ostream& operator<<(std::ostream& os, const ThrowingValue& tv) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return os << GetInstanceString(tv.dummy_);
  }

  friend std::istream& operator>>(std::istream& is, const ThrowingValue&) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    return is;
  }

  // Memory management operators
  // Args.. allows us to overload regular and placement new in one shot
  template <typename... Args>
  static void* operator new(size_t s, Args&&... args) noexcept(
      IsSpecified(TypeSpec::kNoThrowNew)) {
    if (!IsSpecified(TypeSpec::kNoThrowNew)) {
      exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION, true);
    }
    return ::operator new(s, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static void* operator new[](size_t s, Args&&... args) noexcept(
      IsSpecified(TypeSpec::kNoThrowNew)) {
    if (!IsSpecified(TypeSpec::kNoThrowNew)) {
      exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION, true);
    }
    return ::operator new[](s, std::forward<Args>(args)...);
  }

  // Abseil doesn't support throwing overloaded operator delete.  These are
  // provided so a throwing operator-new can clean up after itself.
  //
  // We provide both regular and templated operator delete because if only the
  // templated version is provided as we did with operator new, the compiler has
  // no way of knowing which overload of operator delete to call. See
  // http://en.cppreference.com/w/cpp/memory/new/operator_delete and
  // http://en.cppreference.com/w/cpp/language/delete for the gory details.
  void operator delete(void* p) noexcept { ::operator delete(p); }

  template <typename... Args>
  void operator delete(void* p, Args&&... args) noexcept {
    ::operator delete(p, std::forward<Args>(args)...);
  }

  void operator delete[](void* p) noexcept { return ::operator delete[](p); }

  template <typename... Args>
  void operator delete[](void* p, Args&&... args) noexcept {
    return ::operator delete[](p, std::forward<Args>(args)...);
  }

  // Non-standard access to the actual contained value.  No need for this to
  // throw.
  int& Get() noexcept { return dummy_; }
  const int& Get() const noexcept { return dummy_; }

 private:
  static std::string GetInstanceString(int dummy) {
    return absl::StrCat("ThrowingValue<",
                        exceptions_internal::GetSpecString(Spec), ">(", dummy,
                        ")");
  }

  int dummy_;
};
// While not having to do with exceptions, explicitly delete comma operator, to
// make sure we don't use it on user-supplied types.
template <TypeSpec Spec, typename T>
void operator,(const ThrowingValue<Spec>&, T&&) = delete;
template <TypeSpec Spec, typename T>
void operator,(T&&, const ThrowingValue<Spec>&) = delete;

/*
 * Configuration enum for the ThrowingAllocator type that defines behavior for
 * the lifetime of the instance.
 *
 * kEverythingThrows: Calls to the member functions may throw
 * kNoThrowAllocate: Calls to the member functions will not throw
 */
enum class AllocSpec {
  kEverythingThrows = 0,
  kNoThrowAllocate = 1,
};

/*
 * An allocator type which is instrumented to throw at a controlled time, or not
 * to throw, using AllocSpec. The supported settings are the default of every
 * function which is allowed to throw in a conforming allocator possibly
 * throwing, or nothing throws, in line with the ABSL_ALLOCATOR_THROWS
 * configuration macro.
 */
template <typename T, AllocSpec Spec = AllocSpec::kEverythingThrows>
class ThrowingAllocator : private exceptions_internal::TrackedObject {
  static constexpr bool IsSpecified(AllocSpec spec) {
    return static_cast<bool>(Spec & spec);
  }

 public:
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using void_pointer = void*;
  using const_void_pointer = const void*;
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  using is_nothrow =
      std::integral_constant<bool, Spec == AllocSpec::kNoThrowAllocate>;
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;
  using is_always_equal = std::false_type;

  ThrowingAllocator() : TrackedObject(GetInstanceString(next_id_)) {
    exceptions_internal::MaybeThrow(ABSL_PRETTY_FUNCTION);
    dummy_ = std::make_shared<const int>(next_id_++);
  }

  template <typename U>
  ThrowingAllocator(const ThrowingAllocator<U, Spec>& other) noexcept  // NOLINT
      : TrackedObject(GetInstanceString(*other.State())),
        dummy_(other.State()) {}

  // According to C++11 standard [17.6.3.5], Table 28, the move/copy ctors of
  // allocator shall not exit via an exception, thus they are marked noexcept.
  ThrowingAllocator(const ThrowingAllocator& other) noexcept
      : TrackedObject(GetInstanceString(*other.State())),
        dummy_(other.State()) {}

  template <typename U>
  ThrowingAllocator(ThrowingAllocator<U, Spec>&& other) noexcept  // NOLINT
      : TrackedObject(GetInstanceString(*other.State())),
        dummy_(std::move(other.State())) {}

  ThrowingAllocator(ThrowingAllocator&& other) noexcept
      : TrackedObject(GetInstanceString(*other.State())),
        dummy_(std::move(other.State())) {}

  ~ThrowingAllocator() noexcept = default;

  ThrowingAllocator& operator=(const ThrowingAllocator& other) noexcept {
    dummy_ = other.State();
    return *this;
  }

  template <typename U>
  ThrowingAllocator& operator=(
      const ThrowingAllocator<U, Spec>& other) noexcept {
    dummy_ = other.State();
    return *this;
  }

  template <typename U>
  ThrowingAllocator& operator=(ThrowingAllocator<U, Spec>&& other) noexcept {
    dummy_ = std::move(other.State());
    return *this;
  }

  template <typename U>
  struct rebind {
    using other = ThrowingAllocator<U, Spec>;
  };

  pointer allocate(size_type n) noexcept(
      IsSpecified(AllocSpec::kNoThrowAllocate)) {
    ReadStateAndMaybeThrow(ABSL_PRETTY_FUNCTION);
    return static_cast<pointer>(::operator new(n * sizeof(T)));
  }

  pointer allocate(size_type n, const_void_pointer) noexcept(
      IsSpecified(AllocSpec::kNoThrowAllocate)) {
    return allocate(n);
  }

  void deallocate(pointer ptr, size_type) noexcept {
    ReadState();
    ::operator delete(static_cast<void*>(ptr));
  }

  template <typename U, typename... Args>
  void construct(U* ptr, Args&&... args) noexcept(
      IsSpecified(AllocSpec::kNoThrowAllocate)) {
    ReadStateAndMaybeThrow(ABSL_PRETTY_FUNCTION);
    ::new (static_cast<void*>(ptr)) U(std::forward<Args>(args)...);
  }

  template <typename U>
  void destroy(U* p) noexcept {
    ReadState();
    p->~U();
  }

  size_type max_size() const noexcept {
    return std::numeric_limits<difference_type>::max() / sizeof(value_type);
  }

  ThrowingAllocator select_on_container_copy_construction() noexcept(
      IsSpecified(AllocSpec::kNoThrowAllocate)) {
    auto& out = *this;
    ReadStateAndMaybeThrow(ABSL_PRETTY_FUNCTION);
    return out;
  }

  template <typename U>
  bool operator==(const ThrowingAllocator<U, Spec>& other) const noexcept {
    return dummy_ == other.dummy_;
  }

  template <typename U>
  bool operator!=(const ThrowingAllocator<U, Spec>& other) const noexcept {
    return dummy_ != other.dummy_;
  }

  template <typename, AllocSpec>
  friend class ThrowingAllocator;

 private:
  static std::string GetInstanceString(int dummy) {
    return absl::StrCat("ThrowingAllocator<",
                        exceptions_internal::GetSpecString(Spec), ">(", dummy,
                        ")");
  }

  const std::shared_ptr<const int>& State() const { return dummy_; }
  std::shared_ptr<const int>& State() { return dummy_; }

  void ReadState() {
    // we know that this will never be true, but the compiler doesn't, so this
    // should safely force a read of the value.
    if (*dummy_ < 0) std::abort();
  }

  void ReadStateAndMaybeThrow(absl::string_view msg) const {
    if (!IsSpecified(AllocSpec::kNoThrowAllocate)) {
      exceptions_internal::MaybeThrow(
          absl::Substitute("Allocator id $0 threw from $1", *dummy_, msg));
    }
  }

  static int next_id_;
  std::shared_ptr<const int> dummy_;
};

template <typename T, AllocSpec Spec>
int ThrowingAllocator<T, Spec>::next_id_ = 0;

// Tests for resource leaks by attempting to construct a T using args repeatedly
// until successful, using the countdown method.  Side effects can then be
// tested for resource leaks.
template <typename T, typename... Args>
void TestThrowingCtor(Args&&... args) {
  struct Cleanup {
    ~Cleanup() { exceptions_internal::UnsetCountdown(); }
  } c;
  for (int count = 0;; ++count) {
    exceptions_internal::ConstructorTracker ct(count);
    exceptions_internal::SetCountdown(count);
    try {
      T temp(std::forward<Args>(args)...);
      static_cast<void>(temp);
      break;
    } catch (const exceptions_internal::TestException&) {
    }
  }
}

// Tests the nothrow guarantee of the provided nullary operation. If the an
// exception is thrown, the result will be AssertionFailure(). Otherwise, it
// will be AssertionSuccess().
template <typename Operation>
testing::AssertionResult TestNothrowOp(const Operation& operation) {
  struct Cleanup {
    Cleanup() { exceptions_internal::SetCountdown(); }
    ~Cleanup() { exceptions_internal::UnsetCountdown(); }
  } c;
  try {
    operation();
    return testing::AssertionSuccess();
  } catch (const exceptions_internal::TestException&) {
    return testing::AssertionFailure()
           << "TestException thrown during call to operation() when nothrow "
              "guarantee was expected.";
  } catch (...) {
    return testing::AssertionFailure()
           << "Unknown exception thrown during call to operation() when "
              "nothrow guarantee was expected.";
  }
}

namespace exceptions_internal {

// Dummy struct for ExceptionSafetyTester<> partial state.
struct UninitializedT {};

template <typename T>
class DefaultFactory {
 public:
  explicit DefaultFactory(const T& t) : t_(t) {}
  std::unique_ptr<T> operator()() const { return absl::make_unique<T>(t_); }

 private:
  T t_;
};

template <size_t LazyContractsCount, typename LazyFactory,
          typename LazyOperation>
using EnableIfTestable = typename absl::enable_if_t<
    LazyContractsCount != 0 &&
    !std::is_same<LazyFactory, UninitializedT>::value &&
    !std::is_same<LazyOperation, UninitializedT>::value>;

template <typename Factory = UninitializedT,
          typename Operation = UninitializedT, typename... Contracts>
class ExceptionSafetyTester;

}  // namespace exceptions_internal

exceptions_internal::ExceptionSafetyTester<> MakeExceptionSafetyTester();

namespace exceptions_internal {

/*
 * Builds a tester object that tests if performing a operation on a T follows
 * exception safety guarantees. Verification is done via contract assertion
 * callbacks applied to T instances post-throw.
 *
 * Template parameters for ExceptionSafetyTester:
 *
 * - Factory: The factory object (passed in via tester.WithFactory(...) or
 *   tester.WithInitialValue(...)) must be invocable with the signature
 *   `std::unique_ptr<T> operator()() const` where T is the type being tested.
 *   It is used for reliably creating identical T instances to test on.
 *
 * - Operation: The operation object (passsed in via tester.WithOperation(...)
 *   or tester.Test(...)) must be invocable with the signature
 *   `void operator()(T*) const` where T is the type being tested. It is used
 *   for performing steps on a T instance that may throw and that need to be
 *   checked for exception safety. Each call to the operation will receive a
 *   fresh T instance so it's free to modify and destroy the T instances as it
 *   pleases.
 *
 * - Contracts...: The contract assertion callback objects (passed in via
 *   tester.WithContracts(...)) must be invocable with the signature
 *   `testing::AssertionResult operator()(T*) const` where T is the type being
 *   tested. Contract assertion callbacks are provided T instances post-throw.
 *   They must return testing::AssertionSuccess when the type contracts of the
 *   provided T instance hold. If the type contracts of the T instance do not
 *   hold, they must return testing::AssertionFailure. Execution order of
 *   Contracts... is unspecified. They will each individually get a fresh T
 *   instance so they are free to modify and destroy the T instances as they
 *   please.
 */
template <typename Factory, typename Operation, typename... Contracts>
class ExceptionSafetyTester {
 public:
  /*
   * Returns a new ExceptionSafetyTester with an included T factory based on the
   * provided T instance. The existing factory will not be included in the newly
   * created tester instance. The created factory returns a new T instance by
   * copy-constructing the provided const T& t.
   *
   * Preconditions for tester.WithInitialValue(const T& t):
   *
   * - The const T& t object must be copy-constructible where T is the type
   *   being tested. For non-copy-constructible objects, use the method
   *   tester.WithFactory(...).
   */
  template <typename T>
  ExceptionSafetyTester<DefaultFactory<T>, Operation, Contracts...>
  WithInitialValue(const T& t) const {
    return WithFactory(DefaultFactory<T>(t));
  }

  /*
   * Returns a new ExceptionSafetyTester with the provided T factory included.
   * The existing factory will not be included in the newly-created tester
   * instance. This method is intended for use with types lacking a copy
   * constructor. Types that can be copy-constructed should instead use the
   * method tester.WithInitialValue(...).
   */
  template <typename NewFactory>
  ExceptionSafetyTester<absl::decay_t<NewFactory>, Operation, Contracts...>
  WithFactory(const NewFactory& new_factory) const {
    return {new_factory, operation_, contracts_};
  }

  /*
   * Returns a new ExceptionSafetyTester with the provided testable operation
   * included. The existing operation will not be included in the newly created
   * tester.
   */
  template <typename NewOperation>
  ExceptionSafetyTester<Factory, absl::decay_t<NewOperation>, Contracts...>
  WithOperation(const NewOperation& new_operation) const {
    return {factory_, new_operation, contracts_};
  }

  /*
   * Returns a new ExceptionSafetyTester with the provided MoreContracts...
   * combined with the Contracts... that were already included in the instance
   * on which the method was called. Contracts... cannot be removed or replaced
   * once added to an ExceptionSafetyTester instance. A fresh object must be
   * created in order to get an empty Contracts... list.
   *
   * In addition to passing in custom contract assertion callbacks, this method
   * accepts `testing::strong_guarantee` as an argument which checks T instances
   * post-throw against freshly created T instances via operator== to verify
   * that any state changes made during the execution of the operation were
   * properly rolled back.
   */
  template <typename... MoreContracts>
  ExceptionSafetyTester<Factory, Operation, Contracts...,
                        absl::decay_t<MoreContracts>...>
  WithContracts(const MoreContracts&... more_contracts) const {
    return {
        factory_, operation_,
        std::tuple_cat(contracts_, std::tuple<absl::decay_t<MoreContracts>...>(
                                       more_contracts...))};
  }

  /*
   * Returns a testing::AssertionResult that is the reduced result of the
   * exception safety algorithm. The algorithm short circuits and returns
   * AssertionFailure after the first contract callback returns an
   * AssertionFailure. Otherwise, if all contract callbacks return an
   * AssertionSuccess, the reduced result is AssertionSuccess.
   *
   * The passed-in testable operation will not be saved in a new tester instance
   * nor will it modify/replace the existing tester instance. This is useful
   * when each operation being tested is unique and does not need to be reused.
   *
   * Preconditions for tester.Test(const NewOperation& new_operation):
   *
   * - May only be called after at least one contract assertion callback and a
   *   factory or initial value have been provided.
   */
  template <
      typename NewOperation,
      typename = EnableIfTestable<sizeof...(Contracts), Factory, NewOperation>>
  testing::AssertionResult Test(const NewOperation& new_operation) const {
    return TestImpl(new_operation, absl::index_sequence_for<Contracts...>());
  }

  /*
   * Returns a testing::AssertionResult that is the reduced result of the
   * exception safety algorithm. The algorithm short circuits and returns
   * AssertionFailure after the first contract callback returns an
   * AssertionFailure. Otherwise, if all contract callbacks return an
   * AssertionSuccess, the reduced result is AssertionSuccess.
   *
   * Preconditions for tester.Test():
   *
   * - May only be called after at least one contract assertion callback, a
   *   factory or initial value and a testable operation have been provided.
   */
  template <
      typename LazyOperation = Operation,
      typename = EnableIfTestable<sizeof...(Contracts), Factory, LazyOperation>>
  testing::AssertionResult Test() const {
    return TestImpl(operation_, absl::index_sequence_for<Contracts...>());
  }

 private:
  template <typename, typename, typename...>
  friend class ExceptionSafetyTester;

  friend ExceptionSafetyTester<> testing::MakeExceptionSafetyTester();

  ExceptionSafetyTester() {}

  ExceptionSafetyTester(const Factory& f, const Operation& o,
                        const std::tuple<Contracts...>& i)
      : factory_(f), operation_(o), contracts_(i) {}

  template <typename SelectedOperation, size_t... Indices>
  testing::AssertionResult TestImpl(const SelectedOperation& selected_operation,
                                    absl::index_sequence<Indices...>) const {
    // Starting from 0 and counting upwards until one of the exit conditions is
    // hit...
    for (int count = 0;; ++count) {
      exceptions_internal::ConstructorTracker ct(count);

      // Run the full exception safety test algorithm for the current countdown
      auto reduced_res =
          TestAllContractsAtCountdown(factory_, selected_operation, count,
                                      std::get<Indices>(contracts_)...);
      // If there is no value in the optional, no contracts were run because no
      // exception was thrown. This means that the test is complete and the loop
      // can exit successfully.
      if (!reduced_res.has_value()) {
        return testing::AssertionSuccess();
      }
      // If the optional is not empty and the value is falsy, an contract check
      // failed so the test must exit to propegate the failure.
      if (!reduced_res.value()) {
        return reduced_res.value();
      }
      // If the optional is not empty and the value is not falsy, it means
      // exceptions were thrown but the contracts passed so the test must
      // continue to run.
    }
  }

  Factory factory_;
  Operation operation_;
  std::tuple<Contracts...> contracts_;
};

}  // namespace exceptions_internal

/*
 * Constructs an empty ExceptionSafetyTester. All ExceptionSafetyTester
 * objects are immutable and all With[thing] mutation methods return new
 * instances of ExceptionSafetyTester.
 *
 * In order to test a T for exception safety, a factory for that T, a testable
 * operation, and at least one contract callback returning an assertion
 * result must be applied using the respective methods.
 */
inline exceptions_internal::ExceptionSafetyTester<>
MakeExceptionSafetyTester() {
  return {};
}

}  // namespace testing

#endif  // ABSL_BASE_INTERNAL_EXCEPTION_SAFETY_TESTING_H_
