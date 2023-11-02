// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_OPEN_URL_CONTEXT_H_
#define IOS_TESTING_OPEN_URL_CONTEXT_H_

#import <UIKit/UIKit.h>

/*UISceneOpenURLOptions and UIOpenURLContext can't be instantiated directly*/

// This class mirrors fields of UIOpenURLContext which can't be instantiated
// directly.
@interface TestOpenURLContext : NSObject
@property(nonatomic, copy) NSURL* URL;
@property(nonatomic, strong) UISceneOpenURLOptions* options;
@end

// This class mirrors fields of UISceneOpenURLOptions which can't be
// instantiated directly.
@interface TestSceneOpenURLOptions : NSObject
@property(nonatomic) NSString* sourceApplication;
@property(nonatomic, strong) id annotation;
@property(nonatomic) BOOL openInPlace;
@end

#endif  // IOS_TESTING_OPEN_URL_CONTEXT_H_
