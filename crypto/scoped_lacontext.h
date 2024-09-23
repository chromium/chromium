// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SCOPED_LACONTEXT_H_
#define CRYPTO_SCOPED_LACONTEXT_H_

#if defined(__OBJC__)
#import <LocalAuthentication/LocalAuthentication.h>
#endif  // defined(__OBJC__)

#include <memory>

#include "crypto/crypto_export.h"

namespace crypto {

// ScopedLAContext can hold an `LAContext` and is safe to pass around from C++.
// ScopedLAContext functions as a unique pointer. The UI can create one with an
// authenticated LAContext, then pass it down to the platform.
class CRYPTO_EXPORT ScopedLAContext {
 public:
#if defined(__OBJC__)
  // Takes ownership of |lacontext|.
  explicit ScopedLAContext(LAContext* lacontext);
#endif  // defined(__OBJC__)
  ~ScopedLAContext();
  ScopedLAContext(ScopedLAContext&) = delete;
  ScopedLAContext(ScopedLAContext&&);
  ScopedLAContext& operator=(const ScopedLAContext&) = delete;
  ScopedLAContext& operator=(ScopedLAContext&&);

#if defined(__OBJC__)
  // release returns the last `LAContext` passed on construction and drops its
  // reference to it. It is invalid to to call release more than once.
  LAContext* release();
#endif  // defined(__OBJC__)

 private:
  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> storage_;
};

}  // namespace crypto

#endif  // CRYPTO_SCOPED_LACONTEXT_H_
