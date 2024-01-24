// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/web_state_snapshot_info.h"

@implementation WebStateSnapshotInfo {
  base::WeakPtr<web::WebState> _webState;
}

- (instancetype)initWithWebState:(web::WebState*)webState {
  if ((self = [super init])) {
    _webState = webState->GetWeakPtr();
  }
  return self;
}

- (web::WebState*)webState {
  return _webState.get();
}

@end
