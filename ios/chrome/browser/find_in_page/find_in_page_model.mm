// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/find_in_page_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FindInPageModel ()
// Redefined as readwrite.
@property(copy, nonatomic, readwrite) NSString* text;
@end

@implementation FindInPageModel
@synthesize enabled = _enabled;
@synthesize matches = _matches;
@synthesize currentIndex = _currentIndex;
@synthesize currentPoint = _currentPoint;
@synthesize text = _text;

- (void)setEnabled:(BOOL)enabled {
  _enabled = enabled;
  _matches = 0;
  _currentIndex = 0;
  _currentPoint = CGPointZero;
}

- (void)updateQuery:(NSString*)query matches:(NSUInteger)matches {
  if (query)
    self.text = query;
  _matches = matches;
  _currentIndex = 0;
}

- (void)updateIndex:(NSInteger)index atPoint:(CGPoint)point {
  _currentIndex = index;
  _currentPoint = point;
}

@end
