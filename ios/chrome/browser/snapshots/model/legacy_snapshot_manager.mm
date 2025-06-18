// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/legacy_snapshot_manager.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snapshots/model/legacy_snapshot_generator.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_kind.h"
#import "ios/web/public/thread/web_thread.h"

@implementation LegacySnapshotManager {
  // The snapshot storage.
  id<SnapshotStorage> _snapshotStorage;

  // The snapshot generator which is used to generate snapshots.
  LegacySnapshotGenerator* _snapshotGenerator;

  // The unique ID for WebState's snapshot.
  SnapshotIDWrapper* _snapshotID;

  // The timestamp associated to the latest snapshot stored.
  NSDate* _latestCommitedTimestamp;
}

- (instancetype)initWithGenerator:(LegacySnapshotGenerator*)generator
                       snapshotID:(SnapshotID)snapshotID {
  if ((self = [super init])) {
    DCHECK(snapshotID.valid());
    _snapshotGenerator = generator;
    _snapshotID = [[SnapshotIDWrapper alloc] initWithSnapshotID:snapshotID];
    _latestCommitedTimestamp = [NSDate distantPast];
  }
  return self;
}

- (void)retrieveSnaphotWithKind:(SnapshotKind)snapshotKind
                     completion:(void (^)(UIImage*))completion {
  DCHECK(completion);
  switch (snapshotKind) {
    case SnapshotKindColor:
      [self retrieveSnapshot:completion];
      break;

    case SnapshotKindGreyscale:
      [self retrieveGreySnapshot:completion];
      break;
  }
}

- (void)updateSnapshotWithCompletion:(void (^)(UIImage*))completion {
  DCHECK(_snapshotGenerator);

  __weak LegacySnapshotManager* weakSelf = self;

  // Since the snapshotting strategy may change, the order of snapshot updates
  // cannot be guaranteed. To prevent older snapshots from overwriting newer
  // ones, the timestamp of each snapshot request is recorded.
  NSDate* timestamp = [NSDate now];
  void (^wrappedCompletion)(UIImage*) = ^(UIImage* image) {
    // Update the snapshot storage with the latest snapshot. The old image is
    // deleted if `image` is nil.
    [weakSelf updateSnapshotStorageWithImage:image timestamp:timestamp];

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

// Updates the snapshot storage with `snapshot`.
- (void)updateSnapshotStorageWithImage:(UIImage*)snapshot {
  [self updateSnapshotStorageWithImage:snapshot timestamp:[NSDate now]];
}

- (void)setDelegate:(id<SnapshotGeneratorDelegate>)delegate {
  _snapshotGenerator.delegate = delegate;
}

- (void)setStorage:(id<SnapshotStorage>)storage {
  _snapshotStorage = storage;
}

#pragma mark - Private methods

- (void)retrieveSnapshot:(void (^)(UIImage*))callback {
  DCHECK(callback);
  if (!_snapshotStorage) {
    callback(nil);
    return;
  }

  [_snapshotStorage retrieveImageWithSnapshotID:_snapshotID
                                   snapshotKind:SnapshotKindColor
                                     completion:callback];
}

- (void)retrieveGreySnapshot:(void (^)(UIImage*))callback {
  DCHECK(callback);
  if (!_snapshotStorage) {
    callback(nil);
    return;
  }

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

  [_snapshotStorage retrieveImageWithSnapshotID:_snapshotID
                                   snapshotKind:SnapshotKindGreyscale
                                     completion:wrappedCallback];
}

- (void)updateSnapshotStorageWithImage:(UIImage*)snapshot
                             timestamp:(NSDate*)timestamp {
  if (snapshot) {
    if ([timestamp compare:_latestCommitedTimestamp] == NSOrderedAscending) {
      return;
    }
    _latestCommitedTimestamp = timestamp;
    [_snapshotStorage setImage:snapshot withSnapshotID:_snapshotID];
  } else {
    _latestCommitedTimestamp = [NSDate distantPast];
  }
}

@end
