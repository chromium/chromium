// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

// Maximum size in number of elements that the LRU cache can hold before starting to evict elements.
let kLRUCacheMaxCapacity = 6

// Maximum size in number of elements that the LRU cache can hold before starting to evict elements
// when PinnedTabs feature is enabled.
//
// To calculate the cache size number we'll start with the assumption that currently snapshot
// preloading feature "works fine". In the reality it might not be the case for large screen devices
// such as iPad. Another assumption here is that pinned tabs feature requires on average 4 more
// snapshots to be used. Based on that kLRUCacheMaxCapacityForPinnedTabsEnabled is
// kLRUCacheMaxCapacity which "works fine" + on average 4 more snapshots needed for pinned tabs
// feature.
let kLRUCacheMaxCapacityForPinnedTabsEnabled = 10

// A class providing an in-memory and on-disk storage of tab snapshots.
// A snapshot is a full-screen image of the contents of the page at the current scroll offset and
// zoom level, used to stand in for the WKWebView if it has been purged from memory or when quickly
// switching tabs. Persists to disk on a background thread each time a snapshot changes.
@objcMembers public class SnapshotStorage: NSObject {
  // Weak type to store the observers.
  struct Weak<T: AnyObject> {
    weak var value: T?
  }

  // Cache to hold color snapshots in memory. The gray snapshots are not kept in memory at all.
  private let lruCache: SnapshotLRUCache

  // File manager to read/write images from/to the disk.
  private let fileManager: ImageFileManager

  // List of observers to be notified of changes to the snapshot storage.
  private var observers: [Weak<SnapshotStorageObserver>]

  // Designated initializer. `storageDirectoryUrl` is the file path where all images managed by this
  // SnapshotStorage are stored. `storageDirectoryUrl` is not guaranteed to exist. The contents of
  // `storageDirectoryUrl` are entirely managed by this SnapshotStorage.
  //
  // To support renaming the directory where the snapshots are stored, it is possible to pass a
  // non-empty path via `legacyDirectoryUrl`. If present, then it will be moved to
  // `storageDirectoryUrl`.
  //
  // TODO(crbug.com/40942167): Remove `legacyDirectoryUrl` when the storage for all users has been
  // migrated.
  init(storageDirectoryUrl: URL, legacyDirectoryUrl: URL?) {
    // Use the different size of LRUCache when the pinned tabs feature is enabled.
    // The pinned tabs feature is fully enabled on iPhone and disabled on iPad. The condition to
    // determine the cache size should sync with IsPinnedTabsEnabled() in
    // ios/chrome/browser/tabs/model/features.h.
    self.lruCache = SnapshotLRUCache(
      size: UIDevice.current.userInterfaceIdiom != .pad
        ? kLRUCacheMaxCapacityForPinnedTabsEnabled : kLRUCacheMaxCapacity)
    self.fileManager = ImageFileManager(
      storageDirectoryUrl: storageDirectoryUrl, legacyDirectoryUrl: legacyDirectoryUrl)
    self.observers = []

    super.init()

    NotificationCenter.default.addObserver(
      self, selector: #selector(handleLowMemory),
      name: UIApplication.didReceiveMemoryWarningNotification, object: nil)
    NotificationCenter.default.addObserver(
      self, selector: #selector(handleEnterBackground),
      name: UIApplication.didEnterBackgroundNotification, object: nil)
  }

  // Unregisters observers from Notification Center.
  deinit {
    NotificationCenter.default.removeObserver(
      self, name: UIApplication.didReceiveMemoryWarningNotification, object: nil)
    NotificationCenter.default.removeObserver(
      self, name: UIApplication.didEnterBackgroundNotification, object: nil)
  }

  // Retrieves a cached snapshot for the `snapshotID` and return it via the callback if it exists.
  // The callback is guaranteed to be called synchronously if the image is in memory. It will be
  // called asynchronously if the image is on the disk or with nil if the image is not present at
  // all.
  func retrieveImage(snapshotID: SnapshotIDWrapper, completion: @escaping (UIImage?) -> Void) {
    assert(snapshotID.valid(), "Snapshot ID should be valid")
    if let image = self.lruCache.getObject(forKey: snapshotID) {
      completion(image)
      return
    }

    self.fileManager.readImage(snapshotID: snapshotID) { (image) -> Void in
      guard let image = image else {
        completion(nil)
        return
      }
      self.lruCache.setObject(value: image, forKey: snapshotID)
      completion(image)
    }
  }

  // Retrieves a grey snapshot for `snapshotID`. If the color image is already loaded in memory,
  // the grey snapshot will be generated and the callback will be called immediately. It will be
  // called asynchronously if the color image doesn't exist in memory.
  func retrieveGreyImage(snapshotID: SnapshotIDWrapper, completion: @escaping (UIImage?) -> Void) {
    assert(snapshotID.valid(), "Snapshot ID should be valid")
    if let colorImage = self.lruCache.getObject(forKey: snapshotID) {
      completion(UiKitUtils.greyImage(colorImage))
      return
    }

    // Fallback to reading a color image from the disk when there is no color image in the cache.
    self.fileManager.readImage(snapshotID: snapshotID) { (image) -> Void in
      guard let image = image else {
        completion(nil)
        return
      }
      completion(UiKitUtils.greyImage(image))
    }
  }

  // Sets the image in both the LRU cache and the disk.
  func setImage(_ image: UIImage?, snapshotID: SnapshotIDWrapper) {
    guard let image = image, snapshotID.valid() else {
      return
    }

    lruCache.setObject(value: image, forKey: snapshotID)
    fileManager.write(image: image, snapshotID: snapshotID)

    if let cgImage = image.cgImage {
      // TODO(crbug.com/40910912): Fix the miscalculation of IOS.Snapshots.CacheSize.
      // Each image in the cache has the same resolution and hence the same size.
      let imageSizes = cgImage.bytesPerRow * cgImage.height * Int(lruCache.getCount())
      HistogramUtils.recordHistogram("IOS.Snapshots.CacheSize", withMemoryKB: imageSizes / 1024)
    }

    for observer in observers {
      observer.value?.didUpdateSnapshotStorage?(snapshotID: snapshotID)
    }
  }

  // Removes the image from both the LRU cache and the disk.
  func removeImage(snapshotID: SnapshotIDWrapper) {
    lruCache.removeObject(forKey: snapshotID)
    fileManager.removeImage(snapshotID: snapshotID)

    for weakObserver in observers {
      if let observer = weakObserver.value {
        observer.didUpdateSnapshotStorage?(snapshotID: snapshotID)
      }
    }
  }

  // Removes all images from both the LRU cahce and the disk.
  func removeAllImages() {
    lruCache.removeAllObjects()
    fileManager.removeAllImages()
  }

  // Purges the storage of snapshots that are older than `thresholdDate`. The snapshots for
  // `liveSnapshotIDs` will be kept. This will be done asynchronously.
  func purgeImagesOlderThan(thresholdDate: Date, liveSnapshotIDs: [SnapshotIDWrapper]) {
    fileManager.purgeImagesOlderThan(thresholdDate: thresholdDate, liveSnapshotIDs: liveSnapshotIDs)
  }

  // Renames snapshots with names in `oldIDs` to names in `newIDs`. It is a programmatic error if
  // the two array do not have the same length.
  func renameSnapshots(oldIDs: [String], newIDs: [SnapshotIDWrapper]) {
    assert(
      oldIDs.count == newIDs.count, "The number of old snapshot IDs and new IDs should be same")
    fileManager.renameSnapshots(oldIDs: oldIDs, newIDs: newIDs)
  }

  // Moves the on-disk snapshot from the receiver storage to the destination on-disk storage. If
  // the snapshot is also in-memory, it is moved as well.
  func migrateImage(snapshotID: SnapshotIDWrapper, destinationStorage: SnapshotStorage) {
    if let image = lruCache.getObject(forKey: snapshotID) {
      // Copy both on-disk and in-memory versions.
      destinationStorage.setImage(image, snapshotID: snapshotID)
    } else {
      // Copy on-disk.
      guard let oldPath = imagePath(snapshotID: snapshotID) else {
        return
      }
      guard let newPath = destinationStorage.imagePath(snapshotID: snapshotID) else {
        return
      }
      fileManager.copyImage(oldPath: oldPath, newPath: newPath)
    }

    // Remove the snapshot from this storage.
    removeImage(snapshotID: snapshotID)
  }

  // Adds an observer to this snapshot storage.
  func addObserver(_ observer: SnapshotStorageObserver) {
    if observers.contains(where: { $0.value === observer }) {
      return
    }
    observers.append(Weak(value: observer))
  }

  // Removes an observer from this snapshot storage.
  func removeObserver(_ observer: SnapshotStorageObserver) {
    if let index = observers.firstIndex(where: { $0.value === observer }) {
      observers.remove(at: index)
    }
  }

  // Returns the file path of the image for `snapshotID`.
  func imagePath(snapshotID: SnapshotIDWrapper) -> URL? {
    fileManager.imagePath(snapshotID: snapshotID)
  }

  // Removes all UIImages from the cache.
  func handleLowMemory() {
    lruCache.removeAllObjects()
  }

  // Removes all UIImages from the cache.
  func handleEnterBackground() {
    lruCache.removeAllObjects()
  }

  // Returns the maximum size that the cache can have.
  func lruCacheMaxSize() -> Int {
    return lruCache.maxCacheSize()
  }

  // Clears all images from the cache.
  func clearCache() {
    lruCache.removeAllObjects()
  }
}
