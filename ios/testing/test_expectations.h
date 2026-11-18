// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_TEST_EXPECTATIONS_H_
#define IOS_TESTING_TEST_EXPECTATIONS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ui/base/device_form_factor.h"

typedef NS_OPTIONS(NSUInteger, TestExpectationType) {
  TestExpectationTypeNone = 0,
  TestExpectationTypeFailure = 1 << 0,
  TestExpectationTypePass = 1 << 1,
  TestExpectationTypeSkip = 1 << 2,
  TestExpectationTypeCrash = 1 << 3,
};

enum class TestExpectationMatchResult {
  // XCTest reported an unmatched expected failure (the test unexpectedly
  // passed).
  kUnmatched = 0,
  // The issue occurred and matches this expectation.
  kMatched,
  // The issue occurred but does not match this expectation (e.g. crash vs
  // failure mismatch).
  kMismatched,
};

@interface TestExpectationEntry : NSObject
@property(nonatomic, copy) NSString* bug;
@property(nonatomic, assign) TestExpectationType type;
// 1-indexed line number of this expectation entry in the expectations file it
// was defined in.
@property(nonatomic, assign) NSUInteger lineNumber;
// File path where this expectation entry was defined.
@property(nonatomic, copy) NSString* filePath;

// Returns a human-readable description of the expected outcomes (e.g. "fail",
// "crash", "pass or fail").
- (NSString*)expectedOutcomeDescription;

// Returns a console message documenting this test expectation when running the
// test.
- (NSString*)documentationMessage;

// Returns a message clearly stating the unmet expectation (actual vs expected
// outcome) and identifying the line number of the test expectations file.
- (NSString*)unmetExpectationMessageWithActualOutcome:(NSString*)actualOutcome;

// Returns how the given XCTest issue type and description match this
// expectation.
- (TestExpectationMatchResult)matchesIssueType:(XCTIssueType)issueType
                            compactDescription:(NSString*)compactDescription;
@end

// Helper class to manage and check expected failures for XCTests.
@interface TestExpectations : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Returns the shared expectations instance. Lazily initializes from the bundle
// resources if not yet initialized: `test_expectations.txt` is required, and
// `test_expectations_internal.txt` is loaded on top of it when the test bundle
// contains it. Entries from the internal file override entries with the same
// test identifier (class or class/method) from the public file.
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

// Parses additional expectations `content` on top of the already loaded ones,
// as if it were read from `filePath`. Entries override existing entries with
// the same test identifier. Uses the same code path as `+sharedInstance` when
// it loads `test_expectations_internal.txt`, so tests can cover merging without
// bundling expectations files.
- (void)appendContentForTesting:(NSString*)content filePath:(NSString*)filePath;

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
