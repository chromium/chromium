// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/scoped_disable_fast_animation_earl_grey.h"

#import "ios/testing/earl_grey/base_earl_grey_test_case_app_interface.h"

ScopedDisableFastAnimationEarlGrey::ScopedDisableFastAnimationEarlGrey() {
  [BaseEarlGreyTestCaseAppInterface disableFastAnimation];
}

ScopedDisableFastAnimationEarlGrey::~ScopedDisableFastAnimationEarlGrey() {
  [BaseEarlGreyTestCaseAppInterface enableFastAnimation];
}
