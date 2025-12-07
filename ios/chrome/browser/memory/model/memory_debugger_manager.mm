// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/memory/model/memory_debugger_manager.h"

#import "base/functional/bind.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/memory/model/memory_debugger.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

@implementation MemoryDebuggerManager {
  __weak UIView* _debuggerParentView;
  MemoryDebugger* _memoryDebugger;
  BooleanPrefMember _showMemoryDebugger;
}

- (instancetype)initWithView:(UIView*)debuggerParentView
                       prefs:(PrefService*)prefs {
  if ((self = [super init])) {
    _debuggerParentView = debuggerParentView;

    // Set up the callback for when the pref to show/hide the debugger changes.
    __weak MemoryDebuggerManager* weakSelf = self;
    base::RepeatingClosure callback = base::BindRepeating(^{
      MemoryDebuggerManager* strongSelf = weakSelf;
      if (strongSelf) {
        [self onShowMemoryDebuggingToolsChange];
      }
    });
    _showMemoryDebugger.Init(prefs::kShowMemoryDebuggingTools, prefs, callback);
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
  if (_showMemoryDebugger.GetValue()) {
    _memoryDebugger = [[MemoryDebugger alloc] init];
    [_debuggerParentView addSubview:_memoryDebugger];
  } else {
    [self tearDownDebugger];
  }
}

// Tears down the debugger so it can be deallocated.
- (void)tearDownDebugger {
  _showMemoryDebugger.Destroy();
  [_memoryDebugger invalidateTimers];
  [_memoryDebugger removeFromSuperview];
  _memoryDebugger = nil;
}
@end
