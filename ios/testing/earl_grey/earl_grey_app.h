// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_EARL_GREY_APP_H_
#define IOS_TESTING_EARL_GREY_EARL_GREY_APP_H_

// Contains includes and typedefs to allow code to compile under both EarlGrey1
// and EarlGrey2 (App Process).
// TODO(crbug.com/917390): Remove this file once all callers have been converted
// to EG2.

#import <AppFramework/Action/GREYActionsShorthand.h>
#import <AppFramework/Core/GREYElementInteraction.h>
#import <AppFramework/EarlGreyApp.h>
#import <AppFramework/Matcher/GREYMatchersShorthand.h>
#import <AppFramework/Synchronization/GREYSyncAPI.h>
#import <AppFramework/Synchronization/GREYUIThreadExecutor+GREYApp.h>
#import <CommonLib/Error/GREYErrorConstants.h>
#import <CommonLib/GREYAppleInternals.h>

#endif  // IOS_TESTING_EARL_GREY_EARL_GREY_APP_H_
