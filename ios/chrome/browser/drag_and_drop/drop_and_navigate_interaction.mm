// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/drag_and_drop/drop_and_navigate_interaction.h"

#include "ios/chrome/browser/drag_and_drop/drop_and_navigate_delegate.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DropAndNavigateInteraction ()<UIDropInteractionDelegate> {
  __weak id<DropAndNavigateDelegate> _navigationDelegate;
}

// Returns whether |session| contains an object that can generate a navigation.
- (BOOL)canAcceptDropSession:(id<UIDropSession>)session;

@end

@implementation DropAndNavigateInteraction

- (instancetype)initWithDelegate:
    (id<DropAndNavigateDelegate>)navigationDelegate {
  if (self = [super initWithDelegate:self]) {
    _navigationDelegate = navigationDelegate;
  }
  return self;
}

#pragma mark - UIDropInteractionDelegate

- (void)dropInteraction:(UIDropInteraction*)interaction
            performDrop:(id<UIDropSession>)session API_AVAILABLE(ios(11.0)) {
  if ([session canLoadObjectsOfClass:[NSURL class]]) {
    [session loadObjectsOfClass:[NSURL class]
                     completion:^(NSArray<NSURL*>* objects) {
                       GURL url = net::GURLWithNSURL([objects firstObject]);
                       if (url.is_valid()) {
                         [_navigationDelegate URLWasDropped:url];
                       }
                     }];
  }
  // TODO(crbug.com/753424): Accept NSString drops: generate a search query.
}

- (BOOL)dropInteraction:(UIDropInteraction*)interaction
       canHandleSession:(id<UIDropSession>)session {
  return [self canAcceptDropSession:session];
}

- (UIDropProposal*)dropInteraction:(UIDropInteraction*)interaction
                  sessionDidUpdate:(id<UIDropSession>)session {
  UIDropOperation dropOperation = UIDropOperationCancel;
  if ([self canAcceptDropSession:session]) {
    dropOperation = UIDropOperationCopy;
  }
  return [[UIDropProposal alloc] initWithDropOperation:dropOperation];
}

#pragma mark - Private

- (BOOL)canAcceptDropSession:(id<UIDropSession>)session {
  // TODO(crbug.com/753424): Accept NSString drops: generate a search query.
  return [session canLoadObjectsOfClass:[NSURL class]];
}

@end
