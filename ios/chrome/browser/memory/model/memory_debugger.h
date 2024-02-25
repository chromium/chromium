// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MEMORY_MODEL_MEMORY_DEBUGGER_H_
#define IOS_CHROME_BROWSER_MEMORY_MODEL_MEMORY_DEBUGGER_H_

#import <UIKit/UIKit.h>

// A view that contains memory information (e.g. amount of free memory) and
// tools (e.g. trigger memory warning) to help investigate memory issues and
// performance.
//
// The debugger ensures that it remains visible by continuously calling
// bringSubviewToFront on it's parent so it should be added as a subview of the
// the application's window in order to stay visible all the times.
//
// The debugger owns some timers that must be invalidated before it can be
// deallocated so the owner must call `invalidateTimers` before a MemoryDebugger
// instance can be deallocated.
@interface MemoryDebugger : UIView<UITextFieldDelegate>
// Must be called before the object can be deallocated!
- (void)invalidateTimers;
@end

#endif  // IOS_CHROME_BROWSER_MEMORY_MODEL_MEMORY_DEBUGGER_H_
