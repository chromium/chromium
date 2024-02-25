// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/notreached.h"
#import "ios/public/provider/chrome/browser/primes/primes_api.h"

namespace ios {
namespace provider {

bool IsPrimesSupported() {
  // Primes is not supported for tests.
  return false;
}

void PrimesStartLogging() {
  // Primes is not supported for tests.
  NOTREACHED();
}

void PrimesStopLogging() {
  // Primes is not supported for tests.
  NOTREACHED();
}

void PrimesAppReady() {
  // Primes is not supported for tests.
  NOTREACHED();
}

void PrimesTakeMemorySnapshot(NSString* eventName) {
  // Primes is not used by Chromium
  NOTREACHED();
}

}  // namespace provider
}  // namespace ios
