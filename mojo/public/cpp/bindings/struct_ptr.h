// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRUCT_PTR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRUCT_PTR_H_

#include <cstddef>
#include <functional>
#include <memory>
#include <new>

#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/lib/hash_util.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {
namespace internal {

constexpr size_t kHashSeed = 31;

template <typename Struct>
class StructPtrWTFHelper;

template <typename Struct>
class InlinedStructPtrWTFHelper;

}  // namespace internal

// Smart pointer wrapping a mojom structure with move-only semantics.
template <typename S>
class StructPtr {
 public:
  using Struct = S;

  StructPtr() = default;
  StructPtr(std::nullptr_t) {}

  ~StructPtr() = default;

  StructPtr& operator=(std::nullptr_t) {
    reset();
    return *this;
  }

  StructPtr(StructPtr&& other) { Take(&other); }
  StructPtr& operator=(StructPtr&& other) {
    Take(&other);
    return *this;
  }

  template <typename... Args>
  StructPtr(base::in_place_t, Args&&... args)
      : ptr_(new Struct(std::forward<Args>(args)...)) {}

  template <typename U>
  U To() const {
    return TypeConverter<U, StructPtr>::Convert(*this);
  }

  void reset() { ptr_.reset(); }

  bool is_null() const { return !ptr_; }

  Struct& operator*() const {
    DCHECK(ptr_);
    return *ptr_;
  }
  Struct* operator->() const {
    DCHECK(ptr_);
    return ptr_.get();
  }
  Struct* get() const { return ptr_.get(); }

  void Swap(StructPtr* other) { std::swap(ptr_, other->ptr_); }

  // Please note that calling this method will fail compilation if the value
  // type |Struct| doesn't have a Clone() method defined (which usually means
  // that it contains Mojo handles).
  StructPtr Clone() const { return is_null() ? StructPtr() : ptr_->Clone(); }

  // Compares the pointees (which might both be null).
  // TODO(crbug.com/735302): Get rid of Equals in favor of the operator. Same
  // for Hash.
  bool Equals(const StructPtr& other) const {
    if (is_null() || other.is_null())
      return is_null() && other.is_null();
    return ptr_->Equals(*other.ptr_);
  }

  // Hashes based on the pointee (which might be null).
  size_t Hash(size_t seed) const {
    if (is_null())
      return internal::HashCombine(seed, 0);
    return ptr_->Hash(seed);
  }

  explicit operator bool() const { return !is_null(); }

 private:
  friend class internal::StructPtrWTFHelper<Struct>;
  void Take(StructPtr* other) {
    reset();
    Swap(other);
  }

  std::unique_ptr<Struct> ptr_;

  DISALLOW_COPY_AND_ASSIGN(StructPtr);
};

// Designed to be used when Struct is small and copyable.
template <typename S>
class InlinedStructPtr {
 public:
  using Struct = S;

  InlinedStructPtr() = default;
  InlinedStructPtr(std::nullptr_t) {}

  ~InlinedStructPtr() = default;

  InlinedStructPtr& operator=(std::nullptr_t) {
    reset();
    return *this;
  }

  InlinedStructPtr(InlinedStructPtr&& other) noexcept { Take(&other); }
  InlinedStructPtr& operator=(InlinedStructPtr&& other) noexcept {
    Take(&other);
    return *this;
  }

  template <typename... Args>
  InlinedStructPtr(base::in_place_t, Args&&... args)
      : value_(std::forward<Args>(args)...), state_(VALID) {}

  template <typename U>
  U To() const {
    return TypeConverter<U, InlinedStructPtr>::Convert(*this);
  }

  void reset() {
    state_ = NIL;
    value_. ~Struct();
    new (&value_) Struct();
  }

  bool is_null() const { return state_ == NIL; }

  Struct& operator*() const {
    DCHECK(state_ == VALID);
    return value_;
  }
  Struct* operator->() const {
    DCHECK(state_ == VALID);
    return &value_;
  }
  Struct* get() const { return &value_; }

  void Swap(InlinedStructPtr* other) {
    std::swap(value_, other->value_);
    std::swap(state_, other->state_);
  }

  InlinedStructPtr Clone() const {
    return is_null() ? InlinedStructPtr() : value_.Clone();
  }

  // Compares the pointees (which might both be null).
  bool Equals(const InlinedStructPtr& other) const {
    if (is_null() || other.is_null())
      return is_null() && other.is_null();
    return value_.Equals(other.value_);
  }

  // Hashes based on the pointee (which might be null).
  size_t Hash(size_t seed) const {
    if (is_null())
      return internal::HashCombine(seed, 0);
    return value_.Hash(seed);
  }

  explicit operator bool() const { return !is_null(); }

 private:
  friend class internal::InlinedStructPtrWTFHelper<Struct>;
  void Take(InlinedStructPtr* other) {
    reset();
    Swap(other);
  }

  enum State {
    NIL,
    VALID,
    DELETED,  // For use in WTF::HashMap only
  };

  mutable Struct value_;
  State state_ = NIL;

  DISALLOW_COPY_AND_ASSIGN(InlinedStructPtr);
};

namespace internal {

template <typename Struct>
class StructPtrWTFHelper {
 public:
  static bool IsHashTableDeletedValue(const StructPtr<Struct>& value) {
    return value.ptr_.get() == reinterpret_cast<Struct*>(1u);
  }

  static void ConstructDeletedValue(mojo::StructPtr<Struct>& slot) {
    // |slot| refers to a previous, real value that got deleted and had its
    // destructor run, so this is the first time the "deleted value" has its
    // constructor called.
    //
    // Dirty trick: implant an invalid pointer in |ptr_|. Destructor isn't
    // called for deleted buckets, so this is okay.
    new (&slot) StructPtr<Struct>();
    slot.ptr_.reset(reinterpret_cast<Struct*>(1u));
  }
};

template <typename Struct>
class InlinedStructPtrWTFHelper {
 public:
  static bool IsHashTableDeletedValue(const InlinedStructPtr<Struct>& value) {
    return value.state_ == InlinedStructPtr<Struct>::DELETED;
  }

  static void ConstructDeletedValue(mojo::InlinedStructPtr<Struct>& slot) {
    // |slot| refers to a previous, real value that got deleted and had its
    // destructor run, so this is the first time the "deleted value" has its
    // constructor called.
    new (&slot) InlinedStructPtr<Struct>();
    slot.state_ = InlinedStructPtr<Struct>::DELETED;
  }
};

// Convenience type trait so that we can get away with defining the comparison
// operators only once.
template <typename T>
struct IsStructPtrImpl : std::false_type {};

template <typename S>
struct IsStructPtrImpl<StructPtr<S>> : std::true_type {};

template <typename S>
struct IsStructPtrImpl<InlinedStructPtr<S>> : std::true_type {};

}  // namespace internal

template <typename T>
constexpr bool IsStructPtrV = internal::IsStructPtrImpl<std::decay_t<T>>::value;

template <typename Ptr, std::enable_if_t<IsStructPtrV<Ptr>>* = nullptr>
bool operator==(const Ptr& lhs, const Ptr& rhs) {
  return lhs.Equals(rhs);
}

template <typename Ptr, std::enable_if_t<IsStructPtrV<Ptr>>* = nullptr>
bool operator!=(const Ptr& lhs, const Ptr& rhs) {
  return !(lhs == rhs);
}

// Perform a deep comparison if possible. Otherwise treat null pointers less
// than valid pointers.
template <typename Ptr, std::enable_if_t<IsStructPtrV<Ptr>>* = nullptr>
bool operator<(const Ptr& lhs, const Ptr& rhs) {
  if (!lhs || !rhs)
    return bool{lhs} < bool{rhs};
  return *lhs < *rhs;
}

template <typename Ptr, std::enable_if_t<IsStructPtrV<Ptr>>* = nullptr>
bool operator<=(const Ptr& lhs, const Ptr& rhs) {
  return !(rhs < lhs);
}

template <typename Ptr, std::enable_if_t<IsStructPtrV<Ptr>>* = nullptr>
bool operator>(const Ptr& lhs, const Ptr& rhs) {
  return rhs < lhs;
}

template <typename Ptr, std::enable_if_t<IsStructPtrV<Ptr>>* = nullptr>
bool operator>=(const Ptr& lhs, const Ptr& rhs) {
  return !(lhs < rhs);
}

}  // namespace mojo

namespace std {

template <typename T>
struct hash<mojo::StructPtr<T>> {
  size_t operator()(const mojo::StructPtr<T>& value) const {
    return value.Hash(mojo::internal::kHashSeed);
  }
};

template <typename T>
struct hash<mojo::InlinedStructPtr<T>> {
  size_t operator()(const mojo::InlinedStructPtr<T>& value) const {
    return value.Hash(mojo::internal::kHashSeed);
  }
};

}  // namespace std

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRUCT_PTR_H_
