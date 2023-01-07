// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_DISABLED_TEST_MACROS_H_
#define IOS_TESTING_EARL_GREY_DISABLED_TEST_MACROS_H_

#import <Foundation/Foundation.h>

// A macro that forces an Earl Grey test to pass. It should be used to disable
// tests that fail due to a bug. This macro should be used when the
// configuration for which the test should be disabled can only be determined
// at runtime. Disabling at compile-time is always preferred.
// Example:
// - (void)testFoo
// if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
//   EARL_GREY_TEST_DISABLED(@"Disabled on iPad.");
// }
#define EARL_GREY_TEST_DISABLED(message)                                \
  while (true) {                                                        \
    NSLog(@"-- Earl Grey Test Disabled -- %@", message);                \
    return;                                                             \
  }

// A macro that forces an Earl Grey test to pass. This should be used when a
// test fails for a specific configuration because it is not supported, but
// there is no error. This macro should only be used when the configuration for
// which the test should be disabled can only be determined at runtime.
// Disabling at compile-time is always preferred.
// Example:
// - (void)testFoo
// if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
//   EARL_GREY_TEST_SKIPPED(@"Test not supported on iPad.");
// }
#define EARL_GREY_TEST_SKIPPED(message)                                \
  while (true) {                                                       \
    NSLog(@"-- Earl Grey Test Skipped -- %@", message);                \
    return;                                                            \
  }

#endif  // IOS_TESTING_EARL_GREY_DISABLED_TEST_MACROS_H_
