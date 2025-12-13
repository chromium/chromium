// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/web_state_snapshot_info.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"

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

- (void)takeSnapshot:(CGRect)rect callback:(void (^)(UIImage*))callback {
  SnapshotSourceTabHelper* snapshotSource =
      SnapshotSourceTabHelper::FromWebState(_webState.get());
  snapshotSource->TakeSnapshot(rect, base::BindRepeating(callback));
}

- (BOOL)canTakeSnapshot {
  SnapshotSourceTabHelper* snapshotSource =
      SnapshotSourceTabHelper::FromWebState(_webState.get());
  return snapshotSource->CanTakeSnapshot();
}

- (BOOL)isWebUsageEnabled {
  return _webState->IsWebUsageEnabled();
}

- (CrURL*)lastCommittedURL {
  return [[CrURL alloc] initWithGURL:_webState->GetLastCommittedURL()];
}

@end
