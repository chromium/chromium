// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_mediator.h"

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DefaultPageModeMediator ()

@property(nonatomic, weak) id<DefaultPageModeConsumer> consumer;

@end

@implementation DefaultPageModeMediator

- (instancetype)initWithConsumer:(id<DefaultPageModeConsumer>)consumer {
  self = [super init];
  if (self) {
    _consumer = consumer;
  }
  return self;
}

@end
