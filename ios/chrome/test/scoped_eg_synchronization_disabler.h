// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SCOPED_EG_SYNCHRONIZATION_DISABLER_H_
#define IOS_CHROME_TEST_SCOPED_EG_SYNCHRONIZATION_DISABLER_H_

#import <Foundation/Foundation.h>
#include "base/macros.h"

// Disables EarlGrey synchronization in constructor and returns back to the
// original value in destructor.
class ScopedSynchronizationDisabler {
 public:
  ScopedSynchronizationDisabler();
  ~ScopedSynchronizationDisabler();

 private:
  static bool GetEgSynchronizationEnabled();
  static void SetEgSynchronizationEnabled(BOOL flag);

  BOOL saved_eg_synchronization_enabled_value_ = NO;

  DISALLOW_COPY_AND_ASSIGN(ScopedSynchronizationDisabler);
};

#endif  // IOS_CHROME_TEST_SCOPED_EG_SYNCHRONIZATION_DISABLER_H_
