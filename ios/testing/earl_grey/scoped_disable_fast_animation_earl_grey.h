// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_SCOPED_DISABLE_FAST_ANIMATION_EARL_GREY_H_
#define IOS_TESTING_EARL_GREY_SCOPED_DISABLE_FAST_ANIMATION_EARL_GREY_H_

// Helper class to disable EarlGrey's fast animation.
class ScopedDisableFastAnimationEarlGrey {
 public:
  ScopedDisableFastAnimationEarlGrey();
  ~ScopedDisableFastAnimationEarlGrey();
};

#endif  // IOS_TESTING_EARL_GREY_SCOPED_DISABLE_FAST_ANIMATION_EARL_GREY_H_
