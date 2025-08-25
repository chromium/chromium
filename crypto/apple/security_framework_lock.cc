// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/security_framework_lock.h"

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace crypto::apple {

base::Lock& GetSecurityFrameworkLock() {
  static base::NoDestructor<base::Lock> s_lock;
  return *s_lock;
}

}  // namespace crypto::apple
