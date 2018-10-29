// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_document_interaction_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OpenInMenu ()
// Properties redefined as readwrite.
@property(nonatomic) CGRect rect;
@property(nonatomic) UIView* view;
@property(nonatomic) BOOL animated;
@end

@implementation OpenInMenu
@synthesize rect = _rect;
@synthesize view = _view;
@synthesize animated = _animated;
@end

@implementation FakeDocumentInteractionController
@synthesize UTI = _UTI;
@synthesize delegate = _delegate;
@synthesize presentsOpenInMenu = _presentsOpenInMenu;
@synthesize presentedOpenInMenu = _presentedOpenInMenu;

- (instancetype)init {
  self = [super init];
  if (self) {
    _presentsOpenInMenu = YES;
  }
  return self;
}

- (BOOL)presentOpenInMenuFromRect:(CGRect)rect
                           inView:(UIView*)view
                         animated:(BOOL)animated {
  if (!_presentsOpenInMenu)
    return NO;

  _presentedOpenInMenu = [[OpenInMenu alloc] init];
  _presentedOpenInMenu.rect = rect;
  _presentedOpenInMenu.view = view;
  _presentedOpenInMenu.animated = animated;

  return YES;
}

@end
