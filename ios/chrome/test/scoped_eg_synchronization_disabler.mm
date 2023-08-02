// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"

#import "ios/testing/earl_grey/earl_grey_test.h"

ScopedSynchronizationDisabler::ScopedSynchronizationDisabler()
    : saved_eg_synchronization_enabled_value_(GetEgSynchronizationEnabled()) {
  SetEgSynchronizationEnabled(NO);
}

ScopedSynchronizationDisabler::~ScopedSynchronizationDisabler() {
  SetEgSynchronizationEnabled(saved_eg_synchronization_enabled_value_);
}

bool ScopedSynchronizationDisabler::GetEgSynchronizationEnabled() {
  return [[GREYConfiguration sharedConfiguration]
      boolValueForConfigKey:kGREYConfigKeySynchronizationEnabled];
}

void ScopedSynchronizationDisabler::SetEgSynchronizationEnabled(BOOL flag) {
  [[GREYConfiguration sharedConfiguration]
          setValue:@(flag)
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
}
