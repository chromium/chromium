// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/base_xc_test_case.h"

#import "ios/testing/test_expectations.h"

namespace {

struct TestState {
  bool matched_expectation = false;
  bool issue_occurred = false;
};

}  // namespace

@interface BaseXCTestCase () {
  TestState _testState;
}
- (void)applyExpectation:(TestExpectationEntry*)expectation;
// Configures XCTExpectFailureWithOptions for expected failure or crash.
- (void)configureExpectedFailureForExpectation:
    (TestExpectationEntry*)expectation;
// Handles an issue reported by XCTest and determines whether it matches the
// given expectation, updating test state accordingly.
- (BOOL)handleIssue:(XCTIssue*)issue
     forExpectation:(TestExpectationEntry*)expectation;
@end

@implementation BaseXCTestCase

- (void)setUp {
  [super setUp];
  _testState = TestState();
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
  // Print a message to the console documenting the test expectation.
  NSLog(@"%@", [expectation documentationMessage]);

  TestExpectationType type = expectation.type;
  NSString* reason = expectation.bug;
  if (type & TestExpectationTypeSkip) {
    XCTSkip(@"%@", reason);
  }

  if (type & (TestExpectationTypeFailure | TestExpectationTypeCrash)) {
    [self configureExpectedFailureForExpectation:expectation];
  }
}

- (BOOL)handleIssue:(XCTIssue*)issue
     forExpectation:(TestExpectationEntry*)expectation {
  TestExpectationMatchResult result =
      [expectation matchesIssueType:issue.type
                 compactDescription:issue.compactDescription];
  if (result == TestExpectationMatchResult::kMatched) {
    _testState.issue_occurred = true;
    _testState.matched_expectation = true;
    return YES;
  }
  if (result == TestExpectationMatchResult::kMismatched) {
    _testState.issue_occurred = true;
  }
  return NO;
}

- (void)configureExpectedFailureForExpectation:
    (TestExpectationEntry*)expectation {
  TestExpectationType type = expectation.type;
  XCTExpectedFailureOptions* options = [[XCTExpectedFailureOptions alloc] init];
  // If Pass is specified as part of the expectations (e.g. [ Pass Failure
  // ]), the failure is non-strict (meaning it is flaky, and allowed to
  // pass). Otherwise, the failure is strict (meaning the test is expected
  // to fail consistently, and it is a failure if the test passes).
  options.strict = !(type & TestExpectationTypePass);

  __weak BaseXCTestCase* weakSelf = self;
  options.issueMatcher = ^BOOL(XCTIssue* issue) {
    return [weakSelf handleIssue:issue forExpectation:expectation];
  };

  if (options.strict) {
    [self addTeardownBlock:^{
      BaseXCTestCase* strongSelf = weakSelf;
      if (!strongSelf) {
        return;
      }
      if (!strongSelf->_testState.matched_expectation &&
          !strongSelf->_testState.issue_occurred) {
        NSLog(@"%@",
              [expectation unmetExpectationMessageWithActualOutcome:@"Pass"]);
      }
    }];
  }

  XCTExpectFailureWithOptions(expectation.bug, options);
}

@end
