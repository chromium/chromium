// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/share_highlight_command.h"

#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ShareHighlightCommand {
  GURL _URL;
}

- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
               selectedText:(NSString*)selectedText {
  if (self = [super init]) {
    _URL = URL;
    _title = [title copy];
    _selectedText = [selectedText copy];
  }
  return self;
}

@end
