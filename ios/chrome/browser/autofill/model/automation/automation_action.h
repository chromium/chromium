// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOMATION_AUTOMATION_ACTION_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOMATION_AUTOMATION_ACTION_H_

#import <Foundation/Foundation.h>

#include "base/values.h"

// AutomationAction consumes description of actions in base::Value format,
// generated in json by an extension and executes them on the current
// active web page. AutomationAction is an abstract superclass for a class
// cluster, the class method -actionWithValueDict: returns concrete
// subclasses for the various possible actions.
@interface AutomationAction : NSObject

// Returns an concrete instance of a subclass of AutomationAction.
+ (instancetype)actionWithValueDict:(base::Value::Dict)actionDictionary;

// Prevents creating rogue instances, the init methods are private.
- (instancetype)init NS_UNAVAILABLE;

// For subclasses to implement, execute the action. Use GREYAssert in case of
// issue.
- (void)execute;
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOMATION_AUTOMATION_ACTION_H_
