// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_REFERENCE_COUNTED_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_REFERENCE_COUNTED_IMPL_H_

#include "base/memory/ref_counted.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_export.h"

namespace quiche {

class QUIC_EXPORT_PRIVATE QuicheReferenceCountedImpl
    : public base::RefCountedThreadSafe<QuicheReferenceCountedImpl> {
 public:
  QuicheReferenceCountedImpl() = default;

 protected:
  virtual ~QuicheReferenceCountedImpl() = default;

 private:
  friend class base::RefCountedThreadSafe<QuicheReferenceCountedImpl>;
};

template <class T>
class QuicheReferenceCountedPointerImpl {
 public:
  QuicheReferenceCountedPointerImpl() = default;

  // Constructor from raw pointer |p|. This guarantees that the reference count
  // of *p is 1. This should be only called when a new object is created,
  // calling this on an already existent object does not increase its reference
  // count.
  explicit QuicheReferenceCountedPointerImpl(T* p) : refptr_(p) {}

  // Allows implicit conversion from nullptr.
  QuicheReferenceCountedPointerImpl(std::nullptr_t)  // NOLINT
      : refptr_(nullptr) {}

  // Copy and copy conversion constructors. It does not take the reference away
  // from |other| and they each end up with their own reference.
  template <typename U>
  QuicheReferenceCountedPointerImpl(  // NOLINT
      const QuicheReferenceCountedPointerImpl<U>& other)
      : refptr_(other.refptr()) {}
  QuicheReferenceCountedPointerImpl(
      const QuicheReferenceCountedPointerImpl& other)
      : refptr_(other.refptr()) {}

  // Move constructors. After move, it adopts the reference from |other|.
  template <typename U>
  QuicheReferenceCountedPointerImpl(  // NOLINT
      QuicheReferenceCountedPointerImpl<U>&& other)
      : refptr_(std::move(other.refptr())) {}
  QuicheReferenceCountedPointerImpl(QuicheReferenceCountedPointerImpl&& other)
      : refptr_(std::move(other.refptr())) {}

  ~QuicheReferenceCountedPointerImpl() = default;

  // Copy assignments.
  QuicheReferenceCountedPointerImpl& operator=(
      const QuicheReferenceCountedPointerImpl& other) {
    refptr_ = other.refptr();
    return *this;
  }
  template <typename U>
  QuicheReferenceCountedPointerImpl<T>& operator=(
      const QuicheReferenceCountedPointerImpl<U>& other) {
    refptr_ = other.refptr();
    return *this;
  }

  // Move assignments.
  QuicheReferenceCountedPointerImpl& operator=(
      QuicheReferenceCountedPointerImpl&& other) {
    refptr_ = std::move(other.refptr());
    return *this;
  }
  template <typename U>
  QuicheReferenceCountedPointerImpl<T>& operator=(
      QuicheReferenceCountedPointerImpl<U>&& other) {
    refptr_ = std::move(other.refptr());
    return *this;
  }

  explicit operator bool() const { return static_cast<bool>(refptr_); }

  // Assignment operator on raw pointer. Drops a reference to current pointee,
  // if any, and replaces it with |p|. This guarantees that the reference count
  // of *p is 1. This should only be used when a new object is created.  Calling
  // this on an already existent object is undefined behavior according to the
  // API contract (even though the underlying implementation might have a
  // well-defined behavior).
  QuicheReferenceCountedPointerImpl<T>& operator=(T* p) {
    refptr_ = p;
    return *this;
  }
  // Returns the raw pointer with no change in reference count.
  T* get() const { return refptr_.get(); }

  // Accessors for the referenced object.
  // operator*() and operator->() will assert() if there is no current object.
  T& operator*() const { return *refptr_; }
  T* operator->() const {
    assert(refptr_ != nullptr);
    return refptr_.get();
  }

  scoped_refptr<T>& refptr() { return refptr_; }
  const scoped_refptr<T>& refptr() const { return refptr_; }

 private:
  scoped_refptr<T> refptr_;
};

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_REFERENCE_COUNTED_IMPL_H_
