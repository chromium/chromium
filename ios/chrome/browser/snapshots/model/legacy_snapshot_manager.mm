// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/legacy_snapshot_manager.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snapshots/model/legacy_snapshot_generator.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage_wrapper.h"
#import "ios/web/public/thread/web_thread.h"

@implementation LegacySnapshotManager {
  // The unique ID for WebState's snapshot.
  SnapshotID _snapshotID;
}

- (instancetype)initWithGenerator:(LegacySnapshotGenerator*)generator
                       snapshotID:(SnapshotID)snapshotID {
  if ((self = [super init])) {
    DCHECK(snapshotID.valid());
    _snapshotGenerator = generator;
    _snapshotID = snapshotID;
  }
  return self;
}

- (void)retrieveSnapshot:(void (^)(UIImage*))callback {
  DCHECK(callback);
  if (_snapshotStorage) {
    [_snapshotStorage retrieveImageForSnapshotID:_snapshotID callback:callback];
  } else {
    callback(nil);
  }
}

- (void)retrieveGreySnapshot:(void (^)(UIImage*))callback {
  DCHECK(callback);

  __weak LegacySnapshotManager* weakSelf = self;
  __weak LegacySnapshotGenerator* weakGenerator = _snapshotGenerator;
  void (^wrappedCallback)(UIImage*) = ^(UIImage* image) {
    if (!image) {
      image = [weakGenerator generateUIViewSnapshotWithOverlays];
      [weakSelf updateSnapshotStorageWithImage:image];
      if (image) {
        image = GreyImage(image);
      }
    }
    callback(image);
  };

  if (_snapshotStorage) {
    [_snapshotStorage retrieveGreyImageForSnapshotID:_snapshotID
                                            callback:wrappedCallback];
  } else {
    wrappedCallback(nil);
  }
}

- (void)updateSnapshotWithCompletion:(void (^)(UIImage*))completion {
  DCHECK(_snapshotGenerator);

  __weak LegacySnapshotManager* weakSelf = self;
  void (^wrappedCompletion)(UIImage*) = ^(UIImage* image) {
    // Update the snapshot storage with the latest snapshot. The old image is
    // deleted if `image` is nil.
    [weakSelf updateSnapshotStorageWithImage:image];

    if (completion) {
      completion(image);
    }
  };
  [_snapshotGenerator generateSnapshotWithCompletion:wrappedCompletion];
}

- (UIImage*)generateUIViewSnapshot {
  CHECK(_snapshotGenerator);
  return [_snapshotGenerator generateUIViewSnapshot];
}

- (void)removeSnapshot {
  [_snapshotStorage removeImageWithSnapshotID:_snapshotID];
}

- (void)setDelegate:(id<SnapshotGeneratorDelegate>)delegate {
  _snapshotGenerator.delegate = delegate;
}

#pragma mark - Private methods

// Updates the snapshot storage with `snapshot`.
- (void)updateSnapshotStorageWithImage:(UIImage*)snapshot {
  if (snapshot) {
    [_snapshotStorage setImage:snapshot withSnapshotID:_snapshotID];
  } else {
    // Remove any stale snapshot since the snapshot failed.
    [_snapshotStorage removeImageWithSnapshotID:_snapshotID];
  }
}

@end
