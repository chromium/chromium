// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/base_xc_test_case.h"

#import "ios/testing/test_expectations.h"

@interface BaseXCTestCase ()
- (void)applyExpectation:(TestExpectationEntry*)expectation;
@end

@implementation BaseXCTestCase

- (void)setUp {
  [super setUp];
  // Configure expected failure if defined in the expectations file.
  NSString* className = NSStringFromClass([self class]);
  NSString* methodName = nil;
  NSRange range = [self.name rangeOfString:@" "];
  if (range.location != NSNotFound) {
    methodName = [self.name substringFromIndex:range.location + 1];
    methodName = [methodName
        stringByTrimmingCharactersInSet:
            [NSCharacterSet characterSetWithCharactersInString:@"]"]];
  }

  TestExpectationEntry* expectation = [[TestExpectations sharedInstance]
      expectationEntryForTestCase:className
                       methodName:methodName];
  if (expectation) {
    [self applyExpectation:expectation];
  }
}

- (void)applyExpectation:(TestExpectationEntry*)expectation {
  TestExpectationType type = expectation.type;
  NSString* reason = expectation.bug;
  if (type & TestExpectationTypeSkip) {
    XCTSkip(@"%@", reason);
  }

  if (type & (TestExpectationTypeFailure | TestExpectationTypeCrash)) {
    XCTExpectedFailureOptions* options =
        [[XCTExpectedFailureOptions alloc] init];
    // If Pass is specified as part of the expectations (e.g. [ Pass Failure
    // ]), the failure is non-strict (meaning it is flaky, and allowed to
    // pass). Otherwise, the failure is strict (meaning the test is expected
    // to fail consistently, and it is a failure if the test passes).
    options.strict = !(type & TestExpectationTypePass);

    options.issueMatcher = ^BOOL(XCTIssue* issue) {
      BOOL didCrash = issue.type == XCTIssueTypeUncaughtException ||
                      issue.type == XCTIssueTypeThrownError;

      // EarlGrey aborts test execution on assertion failures by raising an
      // EarlGreyInternalTestInterruptException. This is part of the standard
      // failure flow, not an unexpected crash, so classify it as a failure
      // rather than a crash. Check for both the exception class name and
      // the interrupt description because XCTest's formatting of uncaught
      // exceptions can vary across iOS SDK versions.
      if (didCrash) {
        NSString* description = issue.compactDescription;
        if ([description
                containsString:@"EarlGreyInternalTestInterruptException"] ||
            [description
                containsString:@"Immediately halt execution of testcase"]) {
          didCrash = NO;
        }
      }

      if (didCrash) {
        return (type & TestExpectationTypeCrash) != 0;
      } else {
        return (type & TestExpectationTypeFailure) != 0;
      }
    };

    XCTExpectFailureWithOptions(reason, options);
  }
}

@end
