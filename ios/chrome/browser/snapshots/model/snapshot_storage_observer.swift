// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for listening to events occurring to the SnapshotStorage.
@objc protocol SnapshotStorageObserver {
  // Tells the observing object that the storage was updated with a new snapshot corresponding to
  // `snapshotID`.
  @objc optional func didUpdateSnapshotStorage(snapshotID: SnapshotIDWrapper)
}
