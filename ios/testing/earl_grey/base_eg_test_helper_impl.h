// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_EARL_GREY_BASE_EG_TEST_HELPER_IMPL_H_
#define IOS_TESTING_EARL_GREY_BASE_EG_TEST_HELPER_IMPL_H_

#import <Foundation/Foundation.h>

@class EarlGreyImpl;

// Public macro to use in test helpers methods. Usage example:
//
// @interface MyEarlGreyImpl : BaseEGTestHelperImpl
// @end
// @implementation MyEarlGreyImpl
// + (void)loadURL:(const GURL&)URL {
//   NSString* spec = base::SysUTF8ToNSString(URL.spec());
//   EG_TEST_HELPER_ASSERT_NO_ERROR(LoadUrl(spec));
// }
// @end
//
// In this example LoadUrl() must return NSError* on failure and nil on success
// and MyEarlGreyImpl has to be a subclass of BaseEGTestHelperImpl.
//
#define EG_TEST_HELPER_ASSERT_NO_ERROR(__expression) \
  [self failWithError:__expression expression:@"" #__expression];

// Public macro to use in test helpers methods. Usage example:
//
// @interface MyEarlGreyImpl : TestHelperImpl
// @end
// @implementation MyEarlGreyImpl
// - (void)waitForLoadCompletion {
//   EG_TEST_HELPER_ASSERT_TRUE(WaitForLoadCompletion(),
//                              @"Waiting for load completion");
// }
// @end
//
// In this example WaitForLoadCompletion() must return bool indicating success
// and MyEarlGreyImpl has to be a subclass of TestHelperImpl.
//
#define EG_TEST_HELPER_ASSERT_TRUE(__expression, __description) \
  [self fail:!__expression                                      \
       expression:@"" #__expression                             \
      description:__description];

// Base class used for logging the failure. Compiled in Test Process for EG2 and
// EG1. Must not be instantiated directly.
@interface BaseEGTestHelperImpl : NSObject

// Creates BaseEGTestHelperImpl object with preset file name and line number for
// failure. Must be invoked by helper macro and not called directly.
+ (instancetype)invokedFromFile:(NSString*)fileName lineNumber:(int)lineNumber;

- (instancetype)init NS_UNAVAILABLE;

// Raises EarlGrey exception if |error| argument is not nil.
// |error.localizedDescription| is used as exception reason.
// Invoked by EG_TEST_HELPER_ASSERT_NO_ERROR macro and must not be called
// directly.
- (void)failWithError:(NSError*)error expression:(NSString*)expression;

// Raises EarlGrey exception if |fail| argument is YES.
// |description| is used as exception reason.
// Invoked by EG_TEST_HELPER_ASSERT_TRUE macro and must not be called
// directly.
- (void)fail:(BOOL)fail
     expression:(NSString*)expression
    description:(NSString*)description;

// Underlying EarlGreyImpl object created with file and line number passed to
// invokedFromFile:lineNumber:. Subclasses can use this object instead of
// calling methods on EarlGrey to produce exceptions with correct file and line
// numbers.
- (EarlGreyImpl*)earlGrey;

@end

#endif  // IOS_TESTING_EARL_GREY_BASE_EG_TEST_HELPER_IMPL_H_
