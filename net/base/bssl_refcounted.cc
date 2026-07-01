// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/bssl_refcounted.h"

#include <utility>

#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

template <typename T>
BsslRefcounted<T>::BsslRefcounted() = default;

template <typename T>
BsslRefcounted<T>::BsslRefcounted(bssl::UniquePtr<T> ptr)
    : ptr_(std::move(ptr)) {}

template <typename T>
BsslRefcounted<T>::~BsslRefcounted() = default;

template <typename T>
BsslRefcounted<T>::BsslRefcounted(const BsslRefcounted& other)
    : ptr_(bssl::UpRef(other.ptr_)) {}

template <typename T>
BsslRefcounted<T>& BsslRefcounted<T>::operator=(const BsslRefcounted& other) {
  if (this != &other) {
    ptr_ = bssl::UpRef(other.ptr_);
  }
  return *this;
}

template <typename T>
BsslRefcounted<T>::BsslRefcounted(BsslRefcounted&& other) = default;

template <typename T>
BsslRefcounted<T>& BsslRefcounted<T>::operator=(BsslRefcounted&& other) =
    default;

template <typename T>
void BsslRefcounted<T>::reset(T* ptr) {
  ptr_.reset(ptr);
}

// Explicit instantiations.
template class NET_EXPORT BsslRefcounted<SSL_ECH_KEYS>;
template class NET_EXPORT BsslRefcounted<CRYPTO_BUFFER>;

}  // namespace net
