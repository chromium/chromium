// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/chrome_lens_overlay_result_impl.h"

#import "url/gurl.h"

@implementation ChromeLensOverlayResultImpl
@synthesize searchResultURL = _searchResultURL;
@synthesize selectionPreviewImage = _selectionPreviewImage;
@synthesize suggestSignals = _suggestSignals;
@synthesize isTextSelection = _isTextSelection;
@synthesize queryText = _queryText;
@synthesize selectionRect = _selectionRect;

- (instancetype)initWithResultURL:(GURL)searchResultURL
                     previewImage:(UIImage*)previewImage
                   suggestSignals:(NSData*)suggestSignals
                  isTextSelection:(BOOL)isTextSelection
                        queryText:(NSString*)queryText
                    selectionRect:(CGRect)selectionRect {
  self = [super init];
  if (self) {
    _searchResultURL = searchResultURL;
    _selectionPreviewImage = previewImage;
    _suggestSignals = suggestSignals;
    _isTextSelection = isTextSelection;
    _queryText = queryText;
    _selectionRect = selectionRect;
  }
  return self;
}

@end
