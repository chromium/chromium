// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_BSSL_REFCOUNTED_H_
#define NET_BASE_BSSL_REFCOUNTED_H_

#include "net/base/net_export.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace net {

// BsslRefcounted wraps a bssl::UniquePtr and provides copy constructors
// and copy assignment operators by using bssl::UpRef.
//
// This is a workaround for BoringSSL's scopers not being copyable.
// See https://crbug.com/boringssl/431.
template <typename T>
class NET_EXPORT BsslRefcounted {
 public:
  BsslRefcounted();

  // Intentionally allow implicit conversion from bssl::UniquePtr.
  BsslRefcounted(  // NOLINT(google-explicit-constructor)
      bssl::UniquePtr<T> ptr);

  ~BsslRefcounted();

  BsslRefcounted(const BsslRefcounted& other);
  BsslRefcounted& operator=(const BsslRefcounted& other);

  BsslRefcounted(BsslRefcounted&& other);
  BsslRefcounted& operator=(BsslRefcounted&& other);

  // Forward APIs from bssl::UniquePtr.
  T* get() const { return ptr_.get(); }
  explicit operator bool() const { return static_cast<bool>(ptr_); }

  void reset(T* ptr = nullptr);

 private:
  bssl::UniquePtr<T> ptr_;
};

}  // namespace net

#endif  // NET_BASE_BSSL_REFCOUNTED_H_
