// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/notreached.h"
#import "ios/public/provider/chrome/browser/primes/primes_api.h"

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
