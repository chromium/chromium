// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_restoration_scroll_observer.h"

#import "base/check.h"
#import "base/functional/callback.h"

@implementation SessionRestorationScrollObserver {
  base::RepeatingClosure _closure;
}

- (instancetype)initWithClosure:(base::RepeatingClosure)closure {
  if ((self = [super init])) {
    _closure = closure;
    DCHECK(_closure);
  }
  return self;
}

- (void)shutdown {
  _closure.Reset();
}

#pragma mark - CRWWebViewScrollViewProxyObserver

- (void)webViewScrollViewDidEndDragging:(CRWWebViewScrollViewProxy*)proxy
                         willDecelerate:(BOOL)decelerate {
  // Ignore events occuring after -shutdown.
  if (_closure) {
    _closure.Run();
  }
}

- (void)webViewScrollViewDidEndZooming:(CRWWebViewScrollViewProxy*)proxy
                               atScale:(CGFloat)scale {
  // Ignore events occuring after -shutdown.
  if (_closure) {
    _closure.Run();
  }
}

@end
