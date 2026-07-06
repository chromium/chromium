// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_TEST_EXPECTATIONS_H_
#define IOS_TESTING_TEST_EXPECTATIONS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ui/base/device_form_factor.h"

typedef NS_OPTIONS(NSUInteger, TestExpectationType) {
  TestExpectationTypeNone = 0,
  TestExpectationTypeFailure = 1 << 0,
  TestExpectationTypePass = 1 << 1,
  TestExpectationTypeSkip = 1 << 2,
  TestExpectationTypeCrash = 1 << 3,
};

@interface TestExpectationEntry : NSObject
@property(nonatomic, copy) NSString* bug;
@property(nonatomic, assign) TestExpectationType type;
@end

// Helper class to manage and check expected failures for XCTests.
@interface TestExpectations : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Returns the shared expectations instance. Lazily initializes from the bundle
// resource if not yet initialized.
+ (instancetype)sharedInstance;

// Initializes an instance from string content.
- (instancetype)initWithContent:(NSString*)content;

// Returns the active tags set.
- (NSSet<NSString*>*)activeTags;

// Returns the expectation entry for the given test case and method, or nil if
// no expectation exists.
- (TestExpectationEntry*)expectationEntryForTestCase:(NSString*)testClassName
                                          methodName:(NSString*)methodName;

@end

// Testing-only category.
@interface TestExpectations (Testing)

// Initializes and sets the shared instance from string content for testing
// this test expectations infrastructure.
+ (instancetype)sharedInstanceForTesting:(NSString*)content;

// Sets the active tags to override the default system tags for testing this
// test expectations infrastructure.
- (void)setOverrideActiveTagsForTesting:(NSSet<NSString*>*)tags;

// Resets the shared global instance to nil for testing this test expectations
// infrastructure.
+ (void)resetForTesting;

// Generates device tags given a hardware model string and UI device form
// factor.
+ (NSSet<NSString*>*)deviceTagsForHardwareModel:(NSString*)hardwareModel
                                     formFactor:
                                         (ui::DeviceFormFactor)formFactor;

@end

#endif  // IOS_TESTING_TEST_EXPECTATIONS_H_
