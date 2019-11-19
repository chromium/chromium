// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/deprecated/crw_js_injection_manager.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Testing class of JsInjectioManager that has no dependencies.
@interface TestingCRWJSBaseManager : CRWJSInjectionManager
@end

@implementation TestingCRWJSBaseManager

- (NSString*)staticInjectionContent {
  return @"base = {};";
}

@end

// Testing class of JsInjectionManager that has no dependencies.
@interface TestingAnotherCRWJSBaseManager : CRWJSInjectionManager
@end

@implementation TestingAnotherCRWJSBaseManager

- (NSString*)staticInjectionContent {
  return @"anotherbase = {};";
}

@end

// Testing class of JsInjectioManager that has dependencies.
@interface TestingJsManager : CRWJSInjectionManager
@end

@implementation TestingJsManager

- (NSString*)staticInjectionContent {
  return @"base['testingjs'] = {};";
}

@end

// Testing class of JsInjectioManager that has dynamic content.
@interface TestingDynamicJsManager : CRWJSInjectionManager
@end

@implementation TestingDynamicJsManager

- (NSString*)injectionContent {
  static int i = 0;
  return [NSString stringWithFormat:@"dynamic = {}; dynamic['foo'] = %d;", ++i];
}

- (NSString*)staticInjectionContent {
  // This should never be called on a manager that has dynamic content.
  NOTREACHED();
  return nil;
}

@end


// Testing class of JsInjectioManager that has dependencies.
@interface TestingAnotherJsManager : CRWJSInjectionManager
@end

@implementation TestingAnotherJsManager

- (NSString*)staticInjectionContent {
  return @"base['anothertestingjs'] = {};";
}

@end


// Testing class of JsInjectioManager that has nested dependencies.
@interface TestingJsManagerWithNestedDependencies : CRWJSInjectionManager
@end

@implementation TestingJsManagerWithNestedDependencies

- (NSString*)staticInjectionContent {
  return @"base['testingjswithnesteddependencies'] = {};";
}

@end

// Testing class of JsInjectioManager that has nested dependencies.
@interface TestingJsManagerComplex : CRWJSInjectionManager
@end

@implementation TestingJsManagerComplex

- (NSString*)staticInjectionContent {
  return @"base['testingjswithnesteddependencies']['complex'] = {};";
}

@end

#pragma mark -

namespace web {

// Test fixture to test web controller injection.
class JsInjectionManagerTest : public web::WebTestWithWebState {
 protected:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    // Loads a dummy page to prepare JavaScript evaluation.
    NSString* const kPageContent = @"<html><body><div></div></body></html>";
    LoadHtml(kPageContent);
  }
  // Returns the manager of the given class.
  CRWJSInjectionManager* GetInstanceOfClass(Class jsInjectionManagerClass);
  // Returns true if the receiver_ has all the managers in |managers|.
  bool HasReceiverManagers(NSArray* managers);
};

bool JsInjectionManagerTest::HasReceiverManagers(NSArray* manager_classes) {
  NSDictionary* receiver_managers =
      [web_state()->GetJSInjectionReceiver() managers];
  for (Class manager_class in manager_classes) {
    if (![receiver_managers objectForKey:manager_class])
      return false;
  }
  return true;
}

CRWJSInjectionManager* JsInjectionManagerTest::GetInstanceOfClass(
    Class jsInjectionManagerClass) {
  return [web_state()->GetJSInjectionReceiver()
      instanceOfClass:jsInjectionManagerClass];
}

TEST_F(JsInjectionManagerTest, NoDependencies) {
  NSUInteger originalCount =
      [[web_state()->GetJSInjectionReceiver() managers] count];
  CRWJSInjectionManager* manager =
      GetInstanceOfClass([TestingCRWJSBaseManager class]);
  EXPECT_TRUE(manager);
  EXPECT_EQ(originalCount + 1U,
            [[web_state()->GetJSInjectionReceiver() managers] count]);
  EXPECT_TRUE(HasReceiverManagers(@[ [TestingCRWJSBaseManager class] ]));
  EXPECT_FALSE([manager hasBeenInjected]);

  [manager inject];
  EXPECT_TRUE([manager hasBeenInjected]);
}

TEST_F(JsInjectionManagerTest, Dynamic) {
  CRWJSInjectionManager* manager =
      GetInstanceOfClass([TestingDynamicJsManager class]);
  EXPECT_TRUE(manager);

  EXPECT_FALSE([manager hasBeenInjected]);
  [manager inject];
  EXPECT_TRUE([manager hasBeenInjected]);
  // Ensure that content isn't cached.
  EXPECT_NSNE([manager injectionContent], [manager injectionContent]);
}

// Tests that checking for an uninjected presence beacon returns false.
TEST_F(JsInjectionManagerTest, WebControllerCheckForUninjectedScript) {
  EXPECT_FALSE([web_state()->GetJSInjectionReceiver()
      scriptHasBeenInjectedForClass:Nil]);
}

}  // namespace web
