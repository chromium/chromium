// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_EARL_GREY_APP_H_
#define IOS_TESTING_EARL_GREY_EARL_GREY_APP_H_

// Contains includes and typedefs to allow code to compile under both EarlGrey1
// and EarlGrey2 (App Process).
// TODO(crbug.com/917390): Remove this file once all callers have been converted
// to EG2.

#if defined(CHROME_EARL_GREY_1)

#import <EarlGrey/EarlGrey.h>
#import <EarlGrey/GREYAppleInternals.h>
#import <EarlGrey/GREYKeyboard.h>

typedef DescribeToBlock GREYDescribeToBlock;
typedef MatchesBlock GREYMatchesBlock;

// Provides a no-op implementation of an EG2 API that doesn't exist in EG1. This
// helper assumes that it is already being called on the main thread and
// synchronously runs the given |block|.
void grey_dispatch_sync_on_main_thread(void (^block)(void));

#elif defined(CHROME_EARL_GREY_2)

#import <AppFramework/Action/GREYActionsShorthand.h>
#import <AppFramework/Core/GREYElementInteraction.h>
#import <AppFramework/EarlGreyApp.h>
#import <AppFramework/Matcher/GREYMatchersShorthand.h>
#import <AppFramework/Synchronization/GREYSyncAPI.h>
#import <CommonLib/Error/GREYErrorConstants.h>
#import <CommonLib/GREYAppleInternals.h>

#else
#error Must define either CHROME_EARL_GREY_1 or CHROME_EARL_GREY_2.
#endif

#endif  // IOS_TESTING_EARL_GREY_EARL_GREY_APP_H_
