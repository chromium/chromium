// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_MEMORY_WARNING_HELPER_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_MEMORY_WARNING_HELPER_H_

#import <Foundation/Foundation.h>

// Helper for handling memory warnings.
@interface MemoryWarningHelper : NSObject

// The number of memory warnings that have been received in this
// foreground session.
@property(nonatomic, readonly) NSInteger foregroundMemoryWarningCount;

// Frees as much memory as possible and registers that there was a memory
// pressure.
- (void)handleMemoryPressure;

// Resets the foregroundMemoryWarningCount property and the memoryWarningCount
// crash key, setting their value to 0.
- (void)resetForegroundMemoryWarningCount;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_MEMORY_WARNING_HELPER_H_
