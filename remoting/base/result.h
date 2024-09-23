// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_RESULT_H_
#define REMOTING_BASE_RESULT_H_

#include <optional>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

// Result<SuccessType, ErrorType> represents the success or failure of an
// operation, along with either the success value or error details.
//
// It can be convenient to alias Result for specific error types. For example,
// template <SuccessType>
// using PosixResult<SuccessType, int>
//
// PosixResult<size_t> MyRead(int fd, void* buf, size_t count);

// Synopsis:
//
// SuccessType
// ErrorType
//   The success and error types of the Result.
//
// Result()
//   Only present when the success value is default constructible. Default
//   constructs the success value. This is useful for situations like IPC
//   deserialization where a default-constructed instance is created and the
//   actual value is filled in later. In general, prefer using the
//   Result(kSuccessTag) constructor to be explicit.
//
// Result(SuccessTag, Args&&... args)
// Result(ErrorTag, Args&&... args)
//   Direct constructs the success or error value, respectively, with the
//   provided arguments.
//
// Result(T&& success_value)
// Result(T&& error_value)
//   Implicitly constructs either the success or error value. Only callable if
//   T is implicitly convertible to SuccessType or ErrorType but not both.
//
// Result(const Result& other)
// Result(Result&& other)
//   Copy / move constructors. Only present if both SuccessType and ErrorType
//   are copyable (for the first overload) / moveable (for the second).
//
// Result(const Result<S, E>& other)
// Result(Result<S, E>&& other)
//   Conversion constructors from a compatible Result. Only callable if S is
//   copy (for the first overload) / move (for the second) convertible to
//   SuccessType and E is convertible to ErrorType.
//
// Result& operator=(const Result& other)
// Result& operator=(Result&& other)
//   Copy / move assignment. If this!=*other, destroys the current value and
//   copy / move constructs the current value from other. Only present if both
//   SuccessType and ErrorType are copy (for the first overload) / move (for
//   the second) constructible.
//
// Result& operator=(const Result<S, E>& other)
// Result& operator=(Result<S, E>&& other)
//   Conversion assignment. Only callable if S is copy (for the first overload)
//   / move (for the second) convertible to SuccessType and E is copy / move
//   convertible to ErrorType.
//
// EmplaceSuccess(Args&&... args)
// EmplaceError(Args&&... args)
//   Destroys the current value and direct constructs the success or error value
//   with the provided arguments.
//
// Result<NewSuccessType, ErrorType> Map(F&& on_success) const&
// Result<NewSuccessType, ErrorType> Map(F&& on_success) &&
//   If is_success(), calls on_success passing the current value, and returns a
//   new Result using the return value. If is_error(), the error value is passed
//   through to the new result unchanged.
//
// Result<SuccessType, NewErrorType> MapError(F&& on_error) const&
// Result<SuccessType, NewErrorType> MapError(F&& on_error) &&
//   If is_error(), calls on_error passing the current value, and returns a new
//   Result using the return value. If is_success(), the success value is passed
//   through to the new result unchanged.
//
// Result<NewSuccessType, ErrorType> AndThen(F&& on_success) const&
// Result<NewSuccessType, ErrorType> AndThen(F&& on_success) &&
//   If is_success(), calls on_success passing the current value, which should
//   itself return a Result. That Result is then returned, converting an error
//   to ErrorType if necessary. If is_error(), the error value is passed through
//   to the new result unchanged.
//
// Result<NewSuccessType, ErrorType> OrElse(F&& on_error) const&
// Result<NewSuccessType, ErrorType> OrElse(F&& on_error) &&
//   If is_error(), calls on_error passing the current value, which should
//   itself return a Result. That Result is then returned, converting a success
//   value to SuccessType if necessary. If is_success(), the success value is
//   passed through to the new result unchanged.
//
// R Visit(F&& visitor) const&
// R Visit(F&& visitor) &
// R Visit(F&& visitor) &&
//   Calls either success() or error() on the provided visitor depending on the
//   state of the result, passing the value of the corresponding state and
//   returning the value returned by the visitor. success() and error() must
//   return the same type for the overload called. That is
//   success(const SuccessType&) must have the same return type as
//   error(const ErrorType&), but need not be the same as success(SuccessType&)
//   and error(ErrorType&) (if present).
//
// bool is_success() const
// bool is_error() const
//   Check whether the Result currently holds a success value or an error.
//
// SuccessType& success()
// const SuccessType& success() const
//   Retrieve the success value. Undefined behavior if !is_success().
//
// ErrorType& error()
// const ErrorType& error() const
//   Retrieve the error value. Undefined behavior if !is_error().

namespace remoting {

// TODO(joedow): Migrate instances of remoting::Result to base::expected.

// SuccessTag and ErrorTag are used for constructing a Result in the success
// state or error state, respectively.
class SuccessTag {};
class ErrorTag {};
// absl::monostate can be used for SuccessType or ErrorType to indicate that
// there is no data for that state. Thus, Result<SomeType, monostate> is
// somewhat analogous to std::optional<SomeType>, and Result<monostate,
// monostate> is effectively a (2-byte) boolean. Result<monostate, ErrorType>
// can be useful for cases where an operation can fail, but there is no return
// value in the success case.

constexpr SuccessTag kSuccessTag = SuccessTag();
constexpr ErrorTag kErrorTag = ErrorTag();
constexpr absl::monostate kMonostate = absl::monostate();

namespace internal {

template <typename SuccessType, typename ErrorType>
struct ResultStorage {
  // Default constructor.
  ResultStorage() : ResultStorage(kSuccessTag) {}

  // Direct constructors.
  template <typename... Args>
  ResultStorage(SuccessTag, Args&&... args) : is_success(true) {
    new (std::addressof(success)) SuccessType(std::forward<Args>(args)...);
  }

  template <typename... Args>
  ResultStorage(ErrorTag, Args&&... args) : is_success(false) {
    new (std::addressof(error)) ErrorType(std::forward<Args>(args)...);
  }

  // Copy/move constructors.
  ResultStorage(const ResultStorage& other) : is_success(other.is_success) {
    if (is_success) {
      new (std::addressof(success)) SuccessType(other.success);
    } else {
      new (std::addressof(error)) ErrorType(other.error);
    }
  }

  ResultStorage(ResultStorage&& other) : is_success(other.is_success) {
    if (is_success) {
      new (std::addressof(success)) SuccessType(std::move(other.success));
    } else {
      new (std::addressof(error)) ErrorType(std::move(other.error));
    }
  }

  // Conversion constructors.
  template <typename S, typename E>
  ResultStorage(const ResultStorage<S, E>& other)
      : is_success(other.is_success) {
    if (is_success) {
      new (std::addressof(success)) SuccessType(other.success);
    } else {
      new (std::addressof(error)) ErrorType(other.error);
    }
  }

  template <typename S, typename E>
  ResultStorage(ResultStorage<S, E>&& other) : is_success(other.is_success) {
    if (is_success) {
      new (std::addressof(success)) SuccessType(std::move(other.success));
    } else {
      new (std::addressof(error)) ErrorType(std::move(other.error));
    }
  }

  ~ResultStorage() {
    if (is_success) {
      success.~SuccessType();
    } else {
      error.~ErrorType();
    }
  }

  // Assignment.
  ResultStorage& operator=(const ResultStorage& other) {
    if (this == &other) {
      return *this;
    }
    this->~ResultStorage();
    new (this) ResultStorage(other);
    return *this;
  }

  ResultStorage& operator=(ResultStorage&& other) {
    if (this == &other) {
      return *this;
    }
    this->~ResultStorage();
    new (this) ResultStorage(std::move(other));
    return *this;
  }

  union {
    SuccessType success;
    ErrorType error;
  };

  bool is_success;
};

// The following structs are helpers to implement constructor/assign-operator
// overloading. Result defines all five as "default", and then inherits from
// these base classes so the compiler will include or omit each constructor or
// operator as appropriate.
template <bool is_default_constructible>
struct DefaultConstructible {
  constexpr DefaultConstructible() = default;
  constexpr DefaultConstructible(int) {}
};

template <>
struct DefaultConstructible<false> {
  constexpr DefaultConstructible() = delete;
  constexpr DefaultConstructible(int) {}
};

template <bool is_copy_constructible>
struct CopyConstructible {};

template <>
struct CopyConstructible<false> {
  constexpr CopyConstructible() = default;
  constexpr CopyConstructible(const CopyConstructible&) = delete;
  constexpr CopyConstructible(CopyConstructible&&) = default;
  CopyConstructible& operator=(const CopyConstructible&) = default;
  CopyConstructible& operator=(CopyConstructible&&) = default;
};

template <bool is_move_constructible>
struct MoveConstructible {};

template <>
struct MoveConstructible<false> {
  constexpr MoveConstructible() = default;
  constexpr MoveConstructible(const MoveConstructible&) = default;
  constexpr MoveConstructible(MoveConstructible&&) = delete;
  MoveConstructible& operator=(const MoveConstructible&) = default;
  MoveConstructible& operator=(MoveConstructible&&) = default;
};

template <bool is_copy_assignable>
struct CopyAssignable {};

template <>
struct CopyAssignable<false> {
  constexpr CopyAssignable() = default;
  constexpr CopyAssignable(const CopyAssignable&) = default;
  constexpr CopyAssignable(CopyAssignable&&) = default;
  CopyAssignable& operator=(const CopyAssignable&) = delete;
  CopyAssignable& operator=(CopyAssignable&&) = default;
};

template <bool is_move_assignable>
struct MoveAssignable {};

template <>
struct MoveAssignable<false> {
  constexpr MoveAssignable() = default;
  constexpr MoveAssignable(const MoveAssignable&) = default;
  constexpr MoveAssignable(MoveAssignable&&) = default;
  MoveAssignable& operator=(const MoveAssignable&) = default;
  MoveAssignable& operator=(MoveAssignable&&) = delete;
};

}  // namespace internal

// TODO(rkjnsn): Add [[nodiscard]] once C++17 is allowed.
template <typename SuccessType_, typename ErrorType_>
class Result : public internal::DefaultConstructible<
                   std::is_default_constructible<SuccessType_>::value>,
               public internal::CopyConstructible<
                   std::is_copy_constructible<SuccessType_>::value &&
                   std::is_copy_constructible<ErrorType_>::value>,
               public internal::MoveConstructible<
                   std::is_move_constructible<SuccessType_>::value &&
                   std::is_move_constructible<ErrorType_>::value>,
               public internal::CopyAssignable<
                   std::is_copy_assignable<SuccessType_>::value &&
                   std::is_copy_assignable<ErrorType_>::value>,
               public internal::MoveAssignable<
                   std::is_move_assignable<SuccessType_>::value &&
                   std::is_move_assignable<ErrorType_>::value> {
 public:
  typedef SuccessType_ SuccessType;
  typedef ErrorType_ ErrorType;

 private:
  template <typename T>
  struct is_convertible_result : public std::false_type {};

  template <typename OtherSuccessType, typename OtherErrorType>
  struct is_convertible_result<Result<OtherSuccessType, OtherErrorType>>
      : public std::integral_constant<
            bool,
            std::is_convertible<OtherSuccessType&&, SuccessType>::value &&
                std::is_convertible<OtherErrorType&&, ErrorType>::value> {};

  typedef internal::DefaultConstructible<
      std::is_default_constructible<SuccessType_>::value>
      DefaultConstructible;

 public:
  // Default constructor. Will default construct the success value. This is for
  // situations like IPC deserialization where a default-constructed instance is
  // created and the actual value is filled in later. In general, prefer using
  // Result(kSuccessTag) constructor to be explicit.
  Result() = default;

  // Direct constructors allow constructing either the SuccessType or ErrorType
  // in place. Usage: Result(kSuccessTag, success_type_constructor_args...) or
  // Result(kErrorTag, error_type_constructor_args...)
  template <typename... Args>
  Result(typename std::enable_if<
             std::is_constructible<SuccessType, Args...>::value,
             SuccessTag>::type,
         Args&&... args)
      : DefaultConstructible(0),
        storage_(kSuccessTag, std::forward<Args>(args)...) {}

  template <typename... Args>
  Result(
      typename std::enable_if<std::is_constructible<ErrorType, Args...>::value,
                              ErrorTag>::type,
      Args&&... args)
      : DefaultConstructible(0),
        storage_(kErrorTag, std::forward<Args>(args)...) {}

  // Allow implicit construction from objects implicitly convertible to
  // SuccessType xor ErrorType.
  template <typename T,
            typename std::enable_if<
                std::is_convertible<T&&, SuccessType>::value &&
                    !std::is_convertible<T&&, ErrorType>::value &&
                    // Prefer move/copy/conversion to member construction.
                    !is_convertible_result<typename std::decay<T>::type>::value,
                int>::type = 0>
  Result(T&& success_value)
      : Result(kSuccessTag, std::forward<T>(success_value)) {}

  template <typename T,
            typename std::enable_if<
                !std::is_convertible<T&&, SuccessType>::value &&
                    std::is_convertible<T&&, ErrorType>::value &&
                    !is_convertible_result<typename std::decay<T>::type>::value,
                int>::type = 0>
  Result(T&& error_value) : Result(kErrorTag, std::forward<T>(error_value)) {}

  // Copy / move constructors.
  Result(const Result& other) = default;
  Result(Result&& other) = default;

  // Conversion constructors.
  template <
      typename OtherSuccessType,
      typename OtherErrorType,
      typename std::enable_if<
          std::is_convertible<const OtherSuccessType&, SuccessType>::value &&
              std::is_convertible<const OtherErrorType&, ErrorType>::value,
          int>::type = 0>
  Result(const Result<OtherSuccessType, OtherErrorType>& other)
      : DefaultConstructible(0), storage_(other.storage_) {}

  template <typename OtherSuccessType,
            typename OtherErrorType,
            typename std::enable_if<
                std::is_convertible<OtherSuccessType&&, SuccessType>::value &&
                    std::is_convertible<OtherErrorType&&, ErrorType>::value,
                int>::type = 0>
  Result(Result<OtherSuccessType, OtherErrorType>&& other)
      : DefaultConstructible(0), storage_(std::move(other.storage_)) {}

  // Assignment.
  Result& operator=(const Result& other) = default;
  Result& operator=(Result&& other) = default;

  // Conversion assignment.
  template <
      typename OtherSuccessType,
      typename OtherErrorType,
      typename std::enable_if<
          std::is_convertible<const OtherSuccessType&, SuccessType>::value &&
              std::is_convertible<const OtherErrorType&, ErrorType>::value,
          int>::type = 0>
  Result& operator=(const Result<OtherSuccessType, OtherErrorType>& other) {
    this->~Result();
    new (this) Result(other);
    return *this;
  }

  template <typename OtherSuccessType,
            typename OtherErrorType,
            typename std::enable_if<
                std::is_convertible<OtherSuccessType&&, SuccessType>::value &&
                    std::is_convertible<OtherErrorType&&, ErrorType>::value,
                int>::type = 0>
  Result& operator=(Result<OtherSuccessType, OtherErrorType>&& other) {
    this->~Result();
    new (this) Result(std::move(other));
    return *this;
  }

  // Emplaces new success value in the result and returns a reference to it.
  template <typename... Args>
  typename std::enable_if<std::is_constructible<SuccessType, Args...>::value,
                          SuccessType&>::type
  EmplaceSuccess(Args&&... args) {
    this->~Result();
    new (this) Result(kSuccessTag, std::forward<Args>(args)...);
    return storage_.success;
  }

  // Emplaces new error value in the result and returns a reference to it.
  template <typename... Args>
  typename std::enable_if<std::is_constructible<ErrorType, Args...>::value,
                          ErrorType&>::type
  EmplaceError(Args&&... args) {
    this->~Result();
    new (this) Result(kErrorTag, std::forward<Args>(args)...);
    return storage_.error;
  }

  // Maps Result<Success, Error> to Result<NewSuccess, Error> by applying the
  // provided Success->NewSuccess functor to the success value, if present. If
  // this Result contains an error, it will be passed through unchanged and the
  // functor will not be called.
  template <typename SuccessFunctor>
  Result<
      typename std::invoke_result<SuccessFunctor&&, const SuccessType&>::type,
      ErrorType>
  Map(SuccessFunctor&& on_success) const& {
    if (storage_.is_success) {
      return {kSuccessTag,
              std::forward<SuccessFunctor>(on_success)(storage_.success)};
    } else {
      return {kErrorTag, storage_.error};
    }
  }

  template <typename SuccessFunctor>
  Result<typename std::invoke_result<SuccessFunctor&&, SuccessType&&>::type,
         ErrorType>
  Map(SuccessFunctor&& on_success) && {
    if (storage_.is_success) {
      return {kSuccessTag, std::forward<SuccessFunctor>(on_success)(
                               std::move(storage_.success))};
    } else {
      return {kErrorTag, std::move(storage_.error)};
    }
  }

  // Maps Result<Success, Error> to Result<Success, NewError> by applying the
  // provided Error->NewError functor to the error value, if present. If this
  // Result contains a success value, it will be passed through unchanged and
  // the functor will not be called.
  template <typename ErrorFunctor>
  Result<SuccessType,
         typename std::invoke_result<ErrorFunctor&&, const ErrorType&>::type>
  MapError(ErrorFunctor&& on_error) const& {
    if (storage_.is_success) {
      return {kSuccessTag, storage_.success};
    } else {
      return {kErrorTag, std::forward<ErrorFunctor>(on_error)(storage_.error)};
    }
  }

  template <typename ErrorFunctor>
  Result<SuccessType,
         typename std::invoke_result<ErrorFunctor&&, ErrorType&&>::type>
  MapError(ErrorFunctor&& on_error) && {
    if (storage_.is_success) {
      return {kSuccessTag, std::move(storage_.success)};
    } else {
      return {kErrorTag,
              std::forward<ErrorFunctor>(on_error)(std::move(storage_.error))};
    }
  }

  // Maps Result<Success, Error> to Result<NewSuccess, Error> by calling the
  // provided Success->Result<NewSuccess, Error> functor with the success value,
  // if present. If this Result contains an error, it will be passed through
  // unchanged and the functor will not be called.
  template <
      typename SuccessFunctor,
      typename ReturnType = typename std::
          invoke_result<SuccessFunctor&&, const SuccessType&>::type,
      typename std::enable_if<
          std::is_convertible<typename ReturnType::ErrorType, ErrorType>::value,
          int>::type = 0>
  Result<typename ReturnType::SuccessType, ErrorType> AndThen(
      SuccessFunctor&& on_success) const& {
    if (storage_.is_success) {
      return std::forward<SuccessFunctor>(on_success)(storage_.success);
    } else {
      return {kErrorTag, storage_.error};
    }
  }

  template <
      typename SuccessFunctor,
      typename ReturnType =
          typename std::invoke_result<SuccessFunctor&&, SuccessType&&>::type,
      typename std::enable_if<
          std::is_convertible<typename ReturnType::ErrorType, ErrorType>::value,
          int>::type = 0>
  Result<typename ReturnType::SuccessType, ErrorType> AndThen(
      SuccessFunctor&& on_success) && {
    if (storage_.is_success) {
      return std::forward<SuccessFunctor>(on_success)(
          std::move(storage_.success));
    } else {
      return {kErrorTag, std::move(storage_.error)};
    }
  }

  // Maps Result<Success, Error> to Result<Success, NewError> by calling the
  // provided Error->Result<Success, NewError> functor with the error value, if
  // present. If this Result contains a success value, it will be passed through
  // unchanged and the functor will not be called.
  template <
      typename ErrorFunctor,
      typename ReturnType =
          typename std::invoke_result<ErrorFunctor&&, const ErrorType&>::type,
      typename std::enable_if<
          std::is_convertible<typename ReturnType::SuccessType,
                              SuccessType>::value,
          int>::type = 0>
  Result<SuccessType, typename ReturnType::ErrorType> OrElse(
      ErrorFunctor&& on_error) const& {
    if (storage_.is_success) {
      return {kSuccessTag, storage_.success};
    } else {
      return std::forward<ErrorFunctor>(on_error)(storage_.error);
    }
  }

  template <typename ErrorFunctor,
            typename ReturnType =
                typename std::invoke_result<ErrorFunctor&&, ErrorType&&>::type,
            typename std::enable_if<
                std::is_convertible<typename ReturnType::SuccessType,
                                    SuccessType>::value,
                int>::type = 0>
  Result<SuccessType, typename ReturnType::ErrorType> OrElse(
      ErrorFunctor&& on_error) && {
    if (storage_.is_success) {
      return {kSuccessTag, std::move(storage_.success)};
    } else {
      return std::forward<ErrorFunctor>(on_error)(std::move(storage_.error));
    }
  }

  // Calls either success() or error() on the provided visitor depending on the
  // state of the result, passing the value of the corresponding state.
  template <
      typename Visitor,
      typename SuccessReturn = decltype(std::declval<Visitor>().success(
          std::declval<const SuccessType&>())),
      typename ErrorReturn = decltype(std::declval<Visitor>().error(
          std::declval<const ErrorType&>())),
      typename std::enable_if<std::is_same<SuccessReturn, ErrorReturn>::value,
                              int>::type = 0>
  SuccessReturn Visit(Visitor&& visitor) const& {
    if (storage_.is_success) {
      return std::forward<Visitor>(visitor).success(storage_.success);
    } else {
      return std::forward<Visitor>(visitor).error(storage_.error);
    }
  }

  template <
      typename Visitor,
      typename SuccessReturn = decltype(std::declval<Visitor>().success(
          std::declval<SuccessType&>())),
      typename ErrorReturn =
          decltype(std::declval<Visitor>().error(std::declval<ErrorType&>())),
      typename std::enable_if<std::is_same<SuccessReturn, ErrorReturn>::value,
                              int>::type = 0>
  SuccessReturn Visit(Visitor&& visitor) & {
    if (storage_.is_success) {
      return std::forward<Visitor>(visitor).success(storage_.success);
    } else {
      return std::forward<Visitor>(visitor).error(storage_.error);
    }
  }

  template <
      typename Visitor,
      typename SuccessReturn = decltype(std::declval<Visitor>().success(
          std::declval<SuccessType&&>())),
      typename ErrorReturn =
          decltype(std::declval<Visitor>().error(std::declval<ErrorType&&>())),
      typename std::enable_if<std::is_same<SuccessReturn, ErrorReturn>::value,
                              int>::type = 0>
  SuccessReturn Visit(Visitor&& visitor) && {
    if (storage_.is_success) {
      return std::forward<Visitor>(visitor).success(
          std::move(storage_.success));
    } else {
      return std::forward<Visitor>(visitor).error(std::move(storage_.error));
    }
  }

  // Accessors

  bool is_success() const { return storage_.is_success; }

  bool is_error() const { return !storage_.is_success; }

  SuccessType& success() {
    DCHECK(storage_.is_success);
    return storage_.success;
  }

  const SuccessType& success() const {
    DCHECK(storage_.is_success);
    return storage_.success;
  }

  ErrorType& error() {
    DCHECK(!storage_.is_success);
    return storage_.error;
  }

  const ErrorType& error() const {
    DCHECK(!storage_.is_success);
    return storage_.error;
  }

  // Allow Result to be treated like an Optional that just happens to have more
  // details in the error case.
  explicit operator bool() const { return storage_.is_success; }

  SuccessType& operator*() { return success(); }
  const SuccessType& operator*() const { return success(); }

  SuccessType* operator->() { return &success(); }
  const SuccessType* operator->() const { return &success(); }

 private:
  internal::ResultStorage<SuccessType, ErrorType> storage_;

  template <typename S, typename E>
  friend class Result;
};

}  // namespace remoting

#endif  // REMOTING_BASE_RESULT_H_
