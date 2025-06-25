// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A protocol that takes care of creating, storing and retrieving snapshots.
@MainActor
@objc public protocol SnapshotManager {
  // Asynchronously retrieves a snapshot for the current page, calling
  // `completion` once it has been retrieved. The image will be nil if
  // the snapshot cannot be retrieved or generated.
  @objc func retrieveSnaphot(kind: SnapshotKind, completion: @escaping (UIImage?) -> Void)

  // Asynchronously generates a new snapshot, update the storage and
  // invokes `completion` with the generated image.
  @objc func updateSnapshot(completion: @escaping ((UIImage?) -> Void))

  // Synchronously generates and returns a new snapshot image with the
  // UIKit-based snapshot API. This does not update the snapshot storage.
  @objc func generateUIViewSnapshot() -> UIImage?

  // Updates the storage with `image`.
  @objc func updateSnapshotStorage(image: UIImage?)

  // Sets the SnapshotGeneratorDelegate used to generate snapshots.
  @objc func setDelegate(_ delegate: SnapshotGeneratorDelegate)

  // Sets the SnapshotStorage used to manager the storage.
  @objc func setStorage(_ storage: SnapshotStorage)
}
