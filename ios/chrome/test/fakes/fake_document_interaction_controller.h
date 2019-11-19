// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_DOCUMENT_INTERACTION_CONTROLLER_H_
#define IOS_CHROME_TEST_FAKES_FAKE_DOCUMENT_INTERACTION_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Holds configuration for the presented Open In... menu.
@interface OpenInMenu : NSObject
// These properties hold arguments passed to |presentOpenInMenu...| methods.
@property(nonatomic, readonly) CGRect rect;
@property(nonatomic, readonly) UIView* view;
@property(nonatomic, readonly) BOOL animated;
@end

// Fake class to stub out UIDocumentInteractionController. This class does not
// present any UI, but simply captures the presentation requests.
@interface FakeDocumentInteractionController : NSObject

// Fake implementations of UIDocumentInteractionController properties:
@property(nonatomic, copy) NSString* UTI;
@property(nonatomic, weak) id delegate;

// Whether or not this controller can present Open In... menu. Defaults to YES.
@property(nonatomic) BOOL presentsOpenInMenu;

// Menu that is currently being presented.
@property(nonatomic, readonly) OpenInMenu* presentedOpenInMenu;

// Fake implementations of UIDocumentInteractionController methods:
- (BOOL)presentOpenInMenuFromRect:(CGRect)rect
                           inView:(UIView*)view
                         animated:(BOOL)animated;


@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_DOCUMENT_INTERACTION_CONTROLLER_H_
