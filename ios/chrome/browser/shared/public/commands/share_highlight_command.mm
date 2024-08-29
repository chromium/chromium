// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/commands/share_highlight_command.h"

#import "url/gurl.h"

@implementation ShareHighlightCommand {
  GURL _URL;
}

- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
               selectedText:(NSString*)selectedText
                 sourceView:(UIView*)sourceView
                 sourceRect:(CGRect)sourceRect {
  if ((self = [super init])) {
    _URL = URL;
    _title = [title copy];
    _selectedText = [selectedText copy];
    _sourceView = sourceView;
    _sourceRect = sourceRect;
  }
  return self;
}

@end
