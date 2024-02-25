// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MEMORY_MODEL_MEMORY_DEBUGGER_MANAGER_H_
#define IOS_CHROME_BROWSER_MEMORY_MODEL_MEMORY_DEBUGGER_MANAGER_H_

#import <Foundation/Foundation.h>

class PrefRegistrySimple;
class PrefService;
@class UIView;

// A class to manage the life cycle of a MemoryDebugger instance.
//
// A MemoryDebugger's existence is controlled by a pref in local state, so the
// MemoryDebuggerManager listens for changes to that pref and instantiates or
// frees the debugger as appropriate.
@interface MemoryDebuggerManager : NSObject
// Designated initializer.
- (instancetype)initWithView:(UIView*)view prefs:(PrefService*)prefs;
// Registers local state preferences.
+ (void)registerLocalState:(PrefRegistrySimple*)registry;
@end

#endif  // IOS_CHROME_BROWSER_MEMORY_MODEL_MEMORY_DEBUGGER_MANAGER_H_
