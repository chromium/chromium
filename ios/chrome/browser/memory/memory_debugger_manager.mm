// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/memory/memory_debugger_manager.h"

#include "base/bind.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/memory/memory_debugger.h"
#import "ios/chrome/browser/pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation MemoryDebuggerManager {
  __weak UIView* debuggerParentView_;
  MemoryDebugger* memoryDebugger_;
  BooleanPrefMember showMemoryDebugger_;
}

- (instancetype)initWithView:(UIView*)debuggerParentView
                       prefs:(PrefService*)prefs {
  if (self = [super init]) {
    debuggerParentView_ = debuggerParentView;

    // Set up the callback for when the pref to show/hide the debugger changes.
    __weak MemoryDebuggerManager* weakSelf = self;
    base::RepeatingClosure callback = base::BindRepeating(^{
      MemoryDebuggerManager* strongSelf = weakSelf;
      if (strongSelf) {
        [self onShowMemoryDebuggingToolsChange];
      }
    });
    showMemoryDebugger_.Init(prefs::kShowMemoryDebuggingTools, prefs, callback);
    // Invoke the pref change callback once to show the debugger on start up,
    // if necessary.
    [self onShowMemoryDebuggingToolsChange];
  }
  return self;
}

- (void)dealloc {
  [self tearDownDebugger];
}

#pragma mark - Pref-handling methods

// Registers local state prefs.
+ (void)registerLocalState:(PrefRegistrySimple*)registry {
  registry->RegisterBooleanPref(prefs::kShowMemoryDebuggingTools, false);
}

// Shows or hides the debugger when the pref changes.
- (void)onShowMemoryDebuggingToolsChange {
  if (showMemoryDebugger_.GetValue()) {
    memoryDebugger_ = [[MemoryDebugger alloc] init];
    [debuggerParentView_ addSubview:memoryDebugger_];
  } else {
    [self tearDownDebugger];
  }
}

// Tears down the debugger so it can be deallocated.
- (void)tearDownDebugger {
  showMemoryDebugger_.Destroy();
  [memoryDebugger_ invalidateTimers];
  [memoryDebugger_ removeFromSuperview];
  memoryDebugger_ = nil;
}
@end
