// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/model/link_to_text_payload.h"

@implementation LinkToTextPayload

- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
               selectedText:(NSString*)selectedText
                 sourceView:(UIView*)sourceView
                 sourceRect:(CGRect)sourceRect {
  DCHECK(URL.is_valid());
  DCHECK(title);
  DCHECK(selectedText);
  DCHECK(sourceView);
  if ((self = [super init])) {
    _URL = URL;
    _title = title;
    _selectedText = selectedText;
    _sourceView = sourceView;
    _sourceRect = sourceRect;
  }
  return self;
}

@end
