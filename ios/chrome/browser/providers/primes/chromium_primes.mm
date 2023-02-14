// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/notreached.h"
#import "ios/public/provider/chrome/browser/primes/primes_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {

bool IsPrimesSupported() {
  // Primes is not used by Chromium
  return false;
}

void PrimesStartLogging() {
  // Primes is not used by Chromium
  NOTREACHED();
}

void PrimesStopLogging() {
  // Primes is not used by Chromium
  NOTREACHED();
}

void PrimesAppReady() {
  // Primes is not used by Chromium
  NOTREACHED();
}

}  // namespace provider
}  // namespace ios
