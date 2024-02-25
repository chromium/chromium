// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PRIMES_PRIMES_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PRIMES_PRIMES_API_H_

#import <Foundation/Foundation.h>

namespace ios {
namespace provider {

// Returns whether Primes logging is supported
bool IsPrimesSupported();

// Start Primes logging
void PrimesStartLogging();

// Stop Primes logging
void PrimesStopLogging();

// Tell Primes to track when the app becomes ready for user interaction
void PrimesAppReady();

void PrimesTakeMemorySnapshot(NSString* eventName);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PRIMES_PRIMES_API_H_
