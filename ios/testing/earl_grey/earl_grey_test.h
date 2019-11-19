// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_EARL_GREY_TEST_H_
#define IOS_TESTING_EARL_GREY_EARL_GREY_TEST_H_

// Contains includes and typedefs to allow code to compile under both EarlGrey1
// and EarlGrey2 (Test Process).

#if defined(CHROME_EARL_GREY_1)

#import <EarlGrey/EarlGrey.h>
#import <EarlGrey/GREYAppleInternals.h>
#import <EarlGrey/GREYKeyboard.h>

typedef DescribeToBlock GREYDescribeToBlock;
typedef MatchesBlock GREYMatchesBlock;

id<GREYMatcher> grey_kindOfClassName(NSString* name);

#elif defined(CHROME_EARL_GREY_2)

#import "ios/third_party/earl_grey2/src/TestLib/EarlGreyImpl/EarlGrey.h"  // nogncheck

#else
#error Must define either CHROME_EARL_GREY_1 or CHROME_EARL_GREY_2.
#endif

#endif  // IOS_TESTING_EARL_GREY_EARL_GREY_TEST_H_
