// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_mediator.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_consumer.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation OmniboxMediator
@synthesize consumer = _consumer;

#pragma mark - OmniboxLeftImageConsumer

- (void)setLeftImageForAutocompleteType:(AutocompleteMatchType::Type)type {
  UIImage* image = GetOmniboxSuggestionIconForAutocompleteMatchType(
      type, /* is_starred */ false);
  [self.consumer updateAutocompleteIcon:image];
}

@end
