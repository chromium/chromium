// Copyright 2020 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// StatusOr<T> is the union of a Status object and a T
// object. StatusOr models the concept of an object that is either a
// usable value, or an error Status explaining why such a value is
// not present. To this end, StatusOr<T> does not allow its Status
// value to be absl::OkStatus().
//
// The primary use-case for StatusOr<T> is as the return value of a
// function which may fail.
//
// Example usage of a StatusOr<T>:
//
//  StatusOr<Foo> result = DoBigCalculationThatCouldFail();
//  if (result.ok()) {
//    result->DoSomethingCool();
//  } else {
//    LOG(ERROR) << result.status();
//  }
//
// Example that is guaranteed to crash if the result holds no value:
//
//  StatusOr<Foo> result = DoBigCalculationThatCouldFail();
//  const Foo& foo = result.value();
//  foo.DoSomethingCool();
//
// Example usage of a StatusOr<std::unique_ptr<T>>:
//
//  StatusOr<std::unique_ptr<Foo>> result = FooFactory::MakeNewFoo(arg);
//  if (!result.ok()) {  // Don't omit .ok()
//    LOG(ERROR) << result.status();
//  } else if (*result == nullptr) {
//    LOG(ERROR) << "Unexpected null pointer";
//  } else {
//    (*result)->DoSomethingCool();
//  }
//
// Example factory implementation returning StatusOr<T>:
//
//  StatusOr<Foo> FooFactory::MakeFoo(int arg) {
//    if (arg <= 0) {
//      return absl::Status(absl::StatusCode::kInvalidArgument,
//                          "Arg must be positive");
//    }
//    return Foo(arg);
//  }
//
// NULL POINTERS
//
// Historically StatusOr<T*> treated null pointers specially. This is no longer
// true -- a StatusOr<T*> can be constructed from a null pointer like any other
// pointer value, and the result will be that ok() returns true and value()
// returns null.

#ifndef ABSL_STATUS_STATUSOR_H_
#define ABSL_STATUS_STATUSOR_H_

#include <exception>
#include <initializer_list>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/meta/type_traits.h"
#include "absl/status/internal/statusor_internal.h"
#include "absl/status/status.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
class BadStatusOrAccess : public std::exception {
 public:
  explicit BadStatusOrAccess(absl::Status status);
  ~BadStatusOrAccess() override;
  const char* what() const noexcept override;
  const absl::Status& status() const;

 private:
  absl::Status status_;
};

// Returned StatusOr objects may not be ignored.
template <typename T>
class ABSL_MUST_USE_RESULT StatusOr;

template <typename T>
class StatusOr : private internal_statusor::StatusOrData<T>,
                 private internal_statusor::CopyCtorBase<T>,
                 private internal_statusor::MoveCtorBase<T>,
                 private internal_statusor::CopyAssignBase<T>,
                 private internal_statusor::MoveAssignBase<T> {
  template <typename U>
  friend class StatusOr;

  typedef internal_statusor::StatusOrData<T> Base;

 public:
  typedef T value_type;

  // Constructs a new StatusOr with Status::UNKNOWN status.  This is marked
  // 'explicit' to try to catch cases like 'return {};', where people think
  // absl::StatusOr<std::vector<int>> will be initialized with an empty vector,
  // instead of a Status::UNKNOWN status.
  explicit StatusOr();

  // StatusOr<T> is copy constructible if T is copy constructible.
  StatusOr(const StatusOr&) = default;
  // StatusOr<T> is copy assignable if T is copy constructible and copy
  // assignable.
  StatusOr& operator=(const StatusOr&) = default;

  // StatusOr<T> is move constructible if T is move constructible.
  StatusOr(StatusOr&&) = default;
  // StatusOr<T> is moveAssignable if T is move constructible and move
  // assignable.
  StatusOr& operator=(StatusOr&&) = default;

  // Converting constructors from StatusOr<U>, when T is constructible from U.
  // To avoid ambiguity, they are disabled if T is also constructible from
  // StatusOr<U>. Explicit iff the corresponding construction of T from U is
  // explicit.
  template <
      typename U,
      absl::enable_if_t<
          absl::conjunction<
              absl::negation<std::is_same<T, U>>,
              std::is_constructible<T, const U&>,
              std::is_convertible<const U&, T>,
              absl::negation<
                  internal_statusor::IsConstructibleOrConvertibleFromStatusOr<
                      T, U>>>::value,
          int> = 0>
  StatusOr(const StatusOr<U>& other)  // NOLINT
      : Base(static_cast<const typename StatusOr<U>::Base&>(other)) {}
  template <
      typename U,
      absl::enable_if_t<
          absl::conjunction<
              absl::negation<std::is_same<T, U>>,
              std::is_constructible<T, const U&>,
              absl::negation<std::is_convertible<const U&, T>>,
              absl::negation<
                  internal_statusor::IsConstructibleOrConvertibleFromStatusOr<
                      T, U>>>::value,
          int> = 0>
  explicit StatusOr(const StatusOr<U>& other)
      : Base(static_cast<const typename StatusOr<U>::Base&>(other)) {}

  template <
      typename U,
      absl::enable_if_t<
          absl::conjunction<
              absl::negation<std::is_same<T, U>>, std::is_constructible<T, U&&>,
              std::is_convertible<U&&, T>,
              absl::negation<
                  internal_statusor::IsConstructibleOrConvertibleFromStatusOr<
                      T, U>>>::value,
          int> = 0>
  StatusOr(StatusOr<U>&& other)  // NOLINT
      : Base(static_cast<typename StatusOr<U>::Base&&>(other)) {}
  template <
      typename U,
      absl::enable_if_t<
          absl::conjunction<
              absl::negation<std::is_same<T, U>>, std::is_constructible<T, U&&>,
              absl::negation<std::is_convertible<U&&, T>>,
              absl::negation<
                  internal_statusor::IsConstructibleOrConvertibleFromStatusOr<
                      T, U>>>::value,
          int> = 0>
  explicit StatusOr(StatusOr<U>&& other)
      : Base(static_cast<typename StatusOr<U>::Base&&>(other)) {}

  // Conversion copy/move assignment operator, T must be constructible and
  // assignable from U. Only enable if T cannot be directly assigned from
  // StatusOr<U>.
  template <
      typename U,
      absl::enable_if_t<
          absl::conjunction<
              absl::negation<std::is_same<T, U>>,
              std::is_constructible<T, const U&>,
              std::is_assignable<T, const U&>,
              absl::negation<
                  internal_statusor::
                      IsConstructibleOrConvertibleOrAssignableFromStatusOr<
                          T, U>>>::value,
          int> = 0>
  StatusOr& operator=(const StatusOr<U>& other) {
    this->Assign(other);
    return *this;
  }
  template <
      typename U,
      absl::enable_if_t<
          absl::conjunction<
              absl::negation<std::is_same<T, U>>, std::is_constructible<T, U&&>,
              std::is_assignable<T, U&&>,
              absl::negation<
                  internal_statusor::
                      IsConstructibleOrConvertibleOrAssignableFromStatusOr<
                          T, U>>>::value,
          int> = 0>
  StatusOr& operator=(StatusOr<U>&& other) {
    this->Assign(std::move(other));
    return *this;
  }

  // Constructs a new StatusOr with a non-ok status. After calling this
  // constructor, this->ok() will be false and calls to value() will CHECK-fail.
  // The constructor also takes any type `U` that is convertible to `Status`.
  //
  // NOTE: Not explicit - we want to use StatusOr<T> as a return
  // value, so it is convenient and sensible to be able to do
  // `return Status()` or `return ConvertibleToStatus()` when the return type
  // is `StatusOr<T>`.
  //
  // REQUIRES: !Status(std::forward<U>(v)).ok(). This requirement is DCHECKed.
  // In optimized builds, passing absl::OkStatus() here will have the effect
  // of passing absl::StatusCode::kInternal as a fallback.
  template <
      typename U = absl::Status,
      absl::enable_if_t<
          absl::conjunction<
              std::is_convertible<U&&, absl::Status>,
              std::is_constructible<absl::Status, U&&>,
              absl::negation<std::is_same<absl::decay_t<U>, absl::StatusOr<T>>>,
              absl::negation<std::is_same<absl::decay_t<U>, T>>,
              absl::negation<std::is_same<absl::decay_t<U>, absl::in_place_t>>,
              absl::negation<internal_statusor::HasConversionOperatorToStatusOr<
                  T, U&&>>>::value,
          int> = 0>
  StatusOr(U&& v) : Base(std::forward<U>(v)) {}

  template <
      typename U = absl::Status,
      absl::enable_if_t<
          absl::conjunction<
              absl::negation<std::is_convertible<U&&, absl::Status>>,
              std::is_constructible<absl::Status, U&&>,
              absl::negation<std::is_same<absl::decay_t<U>, absl::StatusOr<T>>>,
              absl::negation<std::is_same<absl::decay_t<U>, T>>,
              absl::negation<std::is_same<absl::decay_t<U>, absl::in_place_t>>,
              absl::negation<internal_statusor::HasConversionOperatorToStatusOr<
                  T, U&&>>>::value,
          int> = 0>
  explicit StatusOr(U&& v) : Base(std::forward<U>(v)) {}

  template <
      typename U = absl::Status,
      absl::enable_if_t<
          absl::conjunction<
              std::is_convertible<U&&, absl::Status>,
              std::is_constructible<absl::Status, U&&>,
              absl::negation<std::is_same<absl::decay_t<U>, absl::StatusOr<T>>>,
              absl::negation<std::is_same<absl::decay_t<U>, T>>,
              absl::negation<std::is_same<absl::decay_t<U>, absl::in_place_t>>,
              absl::negation<internal_statusor::HasConversionOperatorToStatusOr<
                  T, U&&>>>::value,
          int> = 0>
  StatusOr& operator=(U&& v) {
    this->AssignStatus(std::forward<U>(v));
    return *this;
  }

  // Perfect-forwarding value assignment operator.
  // If `*this` contains a `T` value before the call, the contained value is
  // assigned from `std::forward<U>(v)`; Otherwise, it is directly-initialized
  // from `std::forward<U>(v)`.
  // This function does not participate in overload unless:
  // 1. `std::is_constructible_v<T, U>` is true,
  // 2. `std::is_assignable_v<T&, U>` is true.
  // 3. `std::is_same_v<StatusOr<T>, std::remove_cvref_t<U>>` is false.
  // 4. Assigning `U` to `T` is not ambiguous:
  //  If `U` is `StatusOr<V>` and `T` is constructible and assignable from
  //  both `StatusOr<V>` and `V`, the assignment is considered bug-prone and
  //  ambiguous thus will fail to compile. For example:
  //    StatusOr<bool> s1 = true;  // s1.ok() && *s1 == true
  //    StatusOr<bool> s2 = false;  // s2.ok() && *s2 == false
  //    s1 = s2;  // ambiguous, `s1 = *s2` or `s1 = bool(s2)`?
  template <
      typename U = T,
      typename = typename std::enable_if<absl::conjunction<
          std::is_constructible<T, U&&>, std::is_assignable<T&, U&&>,
          absl::disjunction<
              std::is_same<absl::remove_cv_t<absl::remove_reference_t<U>>, T>,
              absl::conjunction<
                  absl::negation<std::is_convertible<U&&, absl::Status>>,
                  absl::negation<internal_statusor::
                                     HasConversionOperatorToStatusOr<T, U&&>>>>,
          internal_statusor::IsForwardingAssignmentValid<T, U&&>>::value>::type>
  StatusOr& operator=(U&& v) {
    static_assert(
        !absl::conjunction<
            std::is_constructible<T, U&&>, std::is_assignable<T&, U&&>,
            std::is_constructible<absl::Status, U&&>,
            std::is_assignable<absl::Status&, U&&>,
            absl::negation<std::is_same<
                T, absl::remove_cv_t<absl::remove_reference_t<U>>>>>::value,
        "U can assign to both T and Status, will result in semantic change");
    static_assert(
        !absl::conjunction<
            std::is_constructible<T, U&&>, std::is_assignable<T&, U&&>,
            internal_statusor::HasConversionOperatorToStatusOr<T, U&&>,
            absl::negation<std::is_same<
                T, absl::remove_cv_t<absl::remove_reference_t<U>>>>>::value,
        "U can assign to T and convert to StatusOr<T>, will result in semantic "
        "change");
    this->Assign(std::forward<U>(v));
    return *this;
  }

  // Constructs the inner value T in-place using the provided args, using the
  // T(args...) constructor.
  template <typename... Args>
  explicit StatusOr(absl::in_place_t, Args&&... args);
  template <typename U, typename... Args>
  explicit StatusOr(absl::in_place_t, std::initializer_list<U> ilist,
                    Args&&... args);

  // Constructs the inner value T in-place using the provided args, using the
  // T(U) (direct-initialization) constructor. Only valid if T can be
  // constructed from a U. Can accept move or copy constructors. Explicit if
  // U is not convertible to T. To avoid ambiguity, this is disabled if U is
  // a StatusOr<J>, where J is convertible to T.
  template <
      typename U = T,
      absl::enable_if_t<
          absl::conjunction<
              internal_statusor::IsDirectInitializationValid<T, U&&>,
              std::is_constructible<T, U&&>, std::is_convertible<U&&, T>,
              absl::disjunction<
                  std::is_same<absl::remove_cv_t<absl::remove_reference_t<U>>,
                               T>,
                  absl::conjunction<
                      absl::negation<std::is_convertible<U&&, absl::Status>>,
                      absl::negation<
                          internal_statusor::HasConversionOperatorToStatusOr<
                              T, U&&>>>>>::value,
          int> = 0>
  StatusOr(U&& u)  // NOLINT
      : StatusOr(absl::in_place, std::forward<U>(u)) {
    static_assert(
        !absl::conjunction<
            std::is_convertible<U&&, T>, std::is_convertible<U&&, absl::Status>,
            absl::negation<std::is_same<
                T, absl::remove_cv_t<absl::remove_reference_t<U>>>>>::value,
        "U is convertible to both T and Status, will result in semantic "
        "change");
    static_assert(
        !absl::conjunction<
            std::is_convertible<U&&, T>,
            internal_statusor::HasConversionOperatorToStatusOr<T, U&&>,
            absl::negation<std::is_same<
                T, absl::remove_cv_t<absl::remove_reference_t<U>>>>>::value,
        "U can construct T and convert to StatusOr<T>, will result in semantic "
        "change");
  }

  template <
      typename U = T,
      absl::enable_if_t<
          absl::conjunction<
              internal_statusor::IsDirectInitializationValid<T, U&&>,
              absl::disjunction<
                  std::is_same<absl::remove_cv_t<absl::remove_reference_t<U>>,
                               T>,
                  absl::conjunction<
                      absl::negation<std::is_constructible<absl::Status, U&&>>,
                      absl::negation<
                          internal_statusor::HasConversionOperatorToStatusOr<
                              T, U&&>>>>,
              std::is_constructible<T, U&&>,
              absl::negation<std::is_convertible<U&&, T>>>::value,
          int> = 0>
  explicit StatusOr(U&& u)  // NOLINT
      : StatusOr(absl::in_place, std::forward<U>(u)) {
    static_assert(
        !absl::conjunction<
            std::is_constructible<T, U&&>,
            std::is_constructible<absl::Status, U&&>,
            absl::negation<std::is_same<
                T, absl::remove_cv_t<absl::remove_reference_t<U>>>>>::value,
        "U can construct both T and Status, will result in semantic "
        "change");
    static_assert(
        !absl::conjunction<
            std::is_constructible<T, U&&>,
            internal_statusor::HasConversionOperatorToStatusOr<T, U&&>,
            absl::negation<std::is_same<
                T, absl::remove_cv_t<absl::remove_reference_t<U>>>>>::value,
        "U can construct T and convert to StatusOr<T>, will result in semantic "
        "change");
  }

  // Returns this->status().ok()
  ABSL_MUST_USE_RESULT bool ok() const { return this->status_.ok(); }

  // Returns a reference to our status. If this contains a T, then
  // returns absl::OkStatus().
  const Status& status() const &;
  Status status() &&;

  // Returns a reference to the held value if `this->ok()`. Otherwise, throws
  // `absl::BadStatusOrAccess` if exception is enabled, or `LOG(FATAL)` if
  // exception is disabled.
  // If you have already checked the status using `this->ok()`, you probably
  // want to use `operator*()` or `operator->()` to access the value instead of
  // `value`.
  // Note: for value types that are cheap to copy, prefer simple code:
  //
  //   T value = statusor.value();
  //
  // Otherwise, if the value type is expensive to copy, but can be left
  // in the StatusOr, simply assign to a reference:
  //
  //   T& value = statusor.value();  // or `const T&`
  //
  // Otherwise, if the value type supports an efficient move, it can be
  // used as follows:
  //
  //   T value = std::move(statusor).value();
  //
  // The `std::move` on statusor instead of on the whole expression enables
  // warnings about possible uses of the statusor object after the move.
  const T& value() const&;
  T& value() &;
  const T&& value() const&&;
  T&& value() &&;

  // Returns a reference to the current value.
  //
  // REQUIRES: this->ok() == true, otherwise the behavior is undefined.
  //
  // Use this->ok() to verify that there is a current value.
  // Alternatively, see value() for a similar API that guarantees
  // CHECK-failing if there is no current value.
  const T& operator*() const&;
  T& operator*() &;
  const T&& operator*() const&&;
  T&& operator*() &&;

  // Returns a pointer to the current value.
  //
  // REQUIRES: this->ok() == true, otherwise the behavior is undefined.
  //
  // Use this->ok() to verify that there is a current value.
  const T* operator->() const;
  T* operator->();

  // Returns the current value this->ok() == true. Otherwise constructs a value
  // using `default_value`.
  //
  // Unlike `value`, this function returns by value, copying the current value
  // if necessary. If the value type supports an efficient move, it can be used
  // as follows:
  //
  //   T value = std::move(statusor).value_or(def);
  //
  // Unlike with `value`, calling `std::move` on the result of `value_or` will
  // still trigger a copy.
  template <typename U>
  T value_or(U&& default_value) const&;
  template <typename U>
  T value_or(U&& default_value) &&;

  // Ignores any errors. This method does nothing except potentially suppress
  // complaints from any tools that are checking that errors are not dropped on
  // the floor.
  void IgnoreError() const;

  // Reconstructs the inner value T in-place using the provided args, using the
  // T(args...) constructor. Returns reference to the reconstructed `T`.
  template <typename... Args>
  T& emplace(Args&&... args) {
    if (ok()) {
      this->Clear();
      this->MakeValue(std::forward<Args>(args)...);
    } else {
      this->MakeValue(std::forward<Args>(args)...);
      this->status_ = absl::OkStatus();
    }
    return this->data_;
  }

  template <
      typename U, typename... Args,
      absl::enable_if_t<
          std::is_constructible<T, std::initializer_list<U>&, Args&&...>::value,
          int> = 0>
  T& emplace(std::initializer_list<U> ilist, Args&&... args) {
    if (ok()) {
      this->Clear();
      this->MakeValue(ilist, std::forward<Args>(args)...);
    } else {
      this->MakeValue(ilist, std::forward<Args>(args)...);
      this->status_ = absl::OkStatus();
    }
    return this->data_;
  }

 private:
  using internal_statusor::StatusOrData<T>::Assign;
  template <typename U>
  void Assign(const absl::StatusOr<U>& other);
  template <typename U>
  void Assign(absl::StatusOr<U>&& other);
};

template <typename T>
bool operator==(const StatusOr<T>& lhs, const StatusOr<T>& rhs) {
  if (lhs.ok() && rhs.ok()) return *lhs == *rhs;
  return lhs.status() == rhs.status();
}

template <typename T>
bool operator!=(const StatusOr<T>& lhs, const StatusOr<T>& rhs) {
  return !(lhs == rhs);
}

////////////////////////////////////////////////////////////////////////////////
// Implementation details for StatusOr<T>

// TODO(sbenza): avoid the string here completely.
template <typename T>
StatusOr<T>::StatusOr() : Base(Status(absl::StatusCode::kUnknown, "")) {}

template <typename T>
template <typename U>
inline void StatusOr<T>::Assign(const StatusOr<U>& other) {
  if (other.ok()) {
    this->Assign(*other);
  } else {
    this->AssignStatus(other.status());
  }
}

template <typename T>
template <typename U>
inline void StatusOr<T>::Assign(StatusOr<U>&& other) {
  if (other.ok()) {
    this->Assign(*std::move(other));
  } else {
    this->AssignStatus(std::move(other).status());
  }
}
template <typename T>
template <typename... Args>
StatusOr<T>::StatusOr(absl::in_place_t, Args&&... args)
    : Base(absl::in_place, std::forward<Args>(args)...) {}

template <typename T>
template <typename U, typename... Args>
StatusOr<T>::StatusOr(absl::in_place_t, std::initializer_list<U> ilist,
                      Args&&... args)
    : Base(absl::in_place, ilist, std::forward<Args>(args)...) {}

template <typename T>
const Status& StatusOr<T>::status() const & { return this->status_; }
template <typename T>
Status StatusOr<T>::status() && {
  return ok() ? OkStatus() : std::move(this->status_);
}

template <typename T>
const T& StatusOr<T>::value() const& {
  if (!this->ok()) internal_statusor::ThrowBadStatusOrAccess(this->status_);
  return this->data_;
}

template <typename T>
T& StatusOr<T>::value() & {
  if (!this->ok()) internal_statusor::ThrowBadStatusOrAccess(this->status_);
  return this->data_;
}

template <typename T>
const T&& StatusOr<T>::value() const&& {
  if (!this->ok()) {
    internal_statusor::ThrowBadStatusOrAccess(std::move(this->status_));
  }
  return std::move(this->data_);
}

template <typename T>
T&& StatusOr<T>::value() && {
  if (!this->ok()) {
    internal_statusor::ThrowBadStatusOrAccess(std::move(this->status_));
  }
  return std::move(this->data_);
}

template <typename T>
const T& StatusOr<T>::operator*() const& {
  this->EnsureOk();
  return this->data_;
}

template <typename T>
T& StatusOr<T>::operator*() & {
  this->EnsureOk();
  return this->data_;
}

template <typename T>
const T&& StatusOr<T>::operator*() const&& {
  this->EnsureOk();
  return std::move(this->data_);
}

template <typename T>
T&& StatusOr<T>::operator*() && {
  this->EnsureOk();
  return std::move(this->data_);
}

template <typename T>
const T* StatusOr<T>::operator->() const {
  this->EnsureOk();
  return &this->data_;
}

template <typename T>
T* StatusOr<T>::operator->() {
  this->EnsureOk();
  return &this->data_;
}

template <typename T>
template <typename U>
T StatusOr<T>::value_or(U&& default_value) const& {
  if (ok()) {
    return this->data_;
  }
  return std::forward<U>(default_value);
}

template <typename T>
template <typename U>
T StatusOr<T>::value_or(U&& default_value) && {
  if (ok()) {
    return std::move(this->data_);
  }
  return std::forward<U>(default_value);
}

template <typename T>
void StatusOr<T>::IgnoreError() const {
  // no-op
}

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_STATUS_STATUSOR_H_
