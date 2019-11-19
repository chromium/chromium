// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"

#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// EG1 and EG2 have different API to obtain shared GREYConfiguration object.
// This function abstracts the API access.
GREYConfiguration* GetSharedGREYConfiguration() {
#if defined(CHROME_EARL_GREY_1)
  return [GREYConfiguration sharedInstance];
#elif defined(CHROME_EARL_GREY_2)
  return [GREYConfiguration sharedConfiguration];
#else
#error Either CHROME_EARL_GREY_1 or CHROME_EARL_GREY_2 must be defined
#endif
}
}  // namespace

ScopedSynchronizationDisabler::ScopedSynchronizationDisabler()
    : saved_eg_synchronization_enabled_value_(GetEgSynchronizationEnabled()) {
  SetEgSynchronizationEnabled(NO);
}

ScopedSynchronizationDisabler::~ScopedSynchronizationDisabler() {
  SetEgSynchronizationEnabled(saved_eg_synchronization_enabled_value_);
}

bool ScopedSynchronizationDisabler::GetEgSynchronizationEnabled() {
  return [GetSharedGREYConfiguration()
      boolValueForConfigKey:kGREYConfigKeySynchronizationEnabled];
}

void ScopedSynchronizationDisabler::SetEgSynchronizationEnabled(BOOL flag) {
  [GetSharedGREYConfiguration() setValue:@(flag)
                            forConfigKey:kGREYConfigKeySynchronizationEnabled];
}
