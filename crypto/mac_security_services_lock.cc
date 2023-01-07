// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/mac_security_services_lock.h"

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace crypto {

base::Lock& GetMacSecurityServicesLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

}  // namespace crypto
