// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_DEFERRED_INITIALIZATION_TASK_NAMES_H_
#define IOS_CHROME_APP_DEFERRED_INITIALIZATION_TASK_NAMES_H_

#import <Foundation/Foundation.h>

// Name of the block performing the initialization of the preference observers.
extern NSString* const kStartupInitPrefObservers;

// Name of the block resetting the startup attempt count (giving the app some
// time to run without crashing before resetting the counter).
extern NSString* const kStartupResetAttemptCount;

#endif  // IOS_CHROME_APP_DEFERRED_INITIALIZATION_TASK_NAMES_H_
