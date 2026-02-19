// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_scroll_observer.h"
@implementation GeminiScrollObserver {
  base::RepeatingClosure _callback;
}

- (instancetype)initWithScrollCallback:(base::RepeatingClosure)callback {
  self = [super init];
  if (self) {
    _callback = callback;
  }
  return self;
}

#pragma mark - CRWWebViewScrollViewProxyObserver

- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  if (_callback) {
    _callback.Run();
  }
}

@end
