// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

// A protocol providing abstraction for storing/retrieving snapshots.
//
// A snapshot is a full-screen image of the contents of the page at the current scroll offset and
// zoom level, used to stand in for the WKWebView if it has been purged from memory or when quickly
// switching tabs.
@MainActor
@objc public protocol SnapshotStorage {
  // Retrieves a cached snapshot for the `snapshotID` and return it via the callback if it exists.
  // The callback is guaranteed to be called synchronously if the image is in memory. It will be
  // called asynchronously if the image is on the disk or with nil if the image is not present at
  // all.
  @objc func retrieveImage(
    snapshotID: SnapshotIDWrapper, snapshotKind: SnapshotKind,
    completion: @escaping (UIImage?) -> Void)

  // Sets the image in both the LRU cache and the disk.
  @objc func setImage(_ image: UIImage?, withSnapshotID: SnapshotIDWrapper)

  // Removes the image from both the LRU cache and the disk.
  @objc func removeImage(snapshotID: SnapshotIDWrapper)

  // Removes all images from both the LRU cache and the disk.
  @objc func removeAllImages()

  // Purges the storage of snapshots that are older than `thresholdDate`. The snapshots for
  // `liveSnapshotIDs` will be kept. This will be done asynchronously.
  @objc func purgeImagesOlderThan(thresholdDate: Date, liveSnapshotIDs: [SnapshotIDWrapper])

  // Moves the on-disk snapshot from the receiver storage to the destination on-disk storage. If
  // the snapshot is also in-memory, it is moved as well.
  @objc func migrateImage(snapshotID: SnapshotIDWrapper, destinationStorage: SnapshotStorage)

  // Adds an observer to this snapshot storage.
  @objc func addObserver(_ observer: SnapshotStorageObserver)

  // Removes an observer from this snapshot storage.
  @objc func removeObserver(_ observer: SnapshotStorageObserver)

  // Returns the file path of the image for `snapshotID`.
  @objc func imagePath(snapshotID: SnapshotIDWrapper) -> URL?

  // Must be invoked before the instance is deallocated. It is needed to release
  // all references to C++ objects. The receiver will likely soon be deallocated.
  @objc func shutdown()
}
