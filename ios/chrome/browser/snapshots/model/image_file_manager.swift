// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import System
import UIKit

// Represents the image type; color or grey.
enum ImageType {
  case kImageTypeColor, kImageTypeGrey
}

// Highest quality. No compression.
let kJPEGImageQuality: CGFloat = 1.0

// A class to manage images stored in disk.
// Tasks for handling disk (reading an image, writing an image, deleting images, renaming an image,
// etc.) are executed on a background thread. Callbacks to use UI APIs should be called on the main
// thread.
@objcMembers public final class ImageFileManager: NSObject, Sendable {
  // Directory where the images are saved.
  private let storageDirectory: URL
  // Scale for snapshot images. It may be smaller than the screen scale in order
  // to save memory on some devices.
  private let imageScale: ImageScale
  // A group to monitor the progress of tasks running on a main thread.
  private let mainTaskGroup: DispatchGroup
  // A group to monitor the progress of tasks running on a background thread.
  private let backgroundTaskGroup: DispatchGroup
  // A queue to manage the execution of tasks. The "default" QoS (Quality of Service Classes) level
  // falls between user-initiated and utility.
  private let backgroundTaskQueue: DispatchQueue

  // Designated initializer. `storageDirectoryUrl` is the file path where all images managed by this
  // ImageFileManager are stored. `storageDirectoryUrl` is not guaranteed to exist. The contents of
  // `storageDirectoryUrl` are entirely managed by this ImageFileManager.
  init(storageDirectoryUrl: URL) {
    self.storageDirectory = storageDirectoryUrl
    self.imageScale = SnapshotImageScale.imageScaleForDevice()
    self.mainTaskGroup = DispatchGroup()
    self.backgroundTaskGroup = DispatchGroup()
    self.backgroundTaskQueue = DispatchQueue(
      label: "org.chromium.image_file_manager", qos: .default)
    super.init()

    createStorageDirectory(directory: storageDirectoryUrl)

    // TODO(crbug.com/40279302): Delete this logic after a few milestones.
    deleteAllGreyImages(directory: storageDirectoryUrl)
  }

  // Waits until all tasks are completed. This is used for tests.
  func waitForAllTasksForTesting(callback: @escaping () -> Void) {
    backgroundTaskGroup.wait()
    // Do not use `mainTaskGroup.wait()` because it can cause a deadlock.
    mainTaskGroup.notify(queue: DispatchQueue.main) {
      callback()
    }
  }

  // Reads a color image from disk. Reading data for UIImage is executed on the background thread
  // and `completion` is executed on the main thread.
  @MainActor
  func readImage(snapshotID: SnapshotIDWrapper, completion: @escaping @MainActor (UIImage?) -> Void)
  {
    guard
      let imagePath = imagePath(
        snapshotID: snapshotID, imageType: ImageType.kImageTypeColor)
    else {
      completion(nil)
      return
    }

    backgroundTaskGroup.enter()
    backgroundTaskQueue.async(group: backgroundTaskGroup) { [weak self] in
      guard let self = self else { return }
      let image = UIImage(contentsOfFile: imagePath.path)
      // Call the callback on the main thread.
      mainTaskGroup.enter()
      DispatchQueue.main.async { [weak self, image] in
        guard let self = self else { return }
        completion(image)
        // Do not call `backgroundTaskGroup.leave()` here. It causes a deadlock on the main thread
        // if we call `backgroundTaskGroup.wait()` before reaching here.
        mainTaskGroup.leave()
      }
      backgroundTaskGroup.leave()
    }
  }

  // Writes an image to disk.
  func write(image: UIImage?, snapshotID: SnapshotIDWrapper) {
    guard let image = image,
      let imagePath = imagePath(
        snapshotID: snapshotID, imageType: ImageType.kImageTypeColor)
    else {
      return
    }

    backgroundTaskGroup.enter()
    backgroundTaskQueue.async(group: backgroundTaskGroup) { [weak self] in
      guard let self = self else { return }
      guard let data = image.jpegData(compressionQuality: kJPEGImageQuality) else {
        backgroundTaskGroup.leave()
        return
      }
      do {
        try data.write(to: imagePath, options: [.atomic])

        // Encrypt the snapshot file (mostly for Incognito, but can't hurt to
        // always do it).
        try FileManager.default.setAttributes(
          [
            .protectionKey: FileProtectionType.completeUntilFirstUserAuthentication
          ], ofItemAtPath: imagePath.path)
      } catch {
        print("Failed to store an image: \(error)")
      }
      backgroundTaskGroup.leave()
    }
  }

  // Removes an image specified by `snapshotID` from disk.
  func removeImage(snapshotID: SnapshotIDWrapper) {
    guard
      let imagePath = imagePath(
        snapshotID: snapshotID, imageType: ImageType.kImageTypeColor)
    else {
      return
    }

    backgroundTaskGroup.enter()
    backgroundTaskQueue.async(group: backgroundTaskGroup) { [weak self] in
      guard let self = self else { return }
      do {
        if FileManager.default.fileExists(atPath: imagePath.path) {
          try FileManager.default.removeItem(at: imagePath)
        }
      } catch {
        print("Failed to delete an image: \(error)")
      }
      backgroundTaskGroup.leave()
    }
  }

  // Removes all images from disk.
  func removeAllImages() {
    backgroundTaskGroup.enter()
    backgroundTaskQueue.async(group: backgroundTaskGroup) { [weak self] in
      guard let self = self else { return }
      do {
        // Delete the directory storing all images and create a brand new directory with the same
        // storage path.
        try FileManager.default.removeItem(at: storageDirectory)
        try FileManager.default.createDirectory(
          at: storageDirectory, withIntermediateDirectories: true)
      } catch {
        print("Failed to delete an image: \(error)")
      }
      backgroundTaskGroup.leave()
    }
  }

  // Purges the storage of snapshots that are older than `date`. The snapshots for `liveSnapshotIDs`
  // will be kept. This will be done asynchronously on a background thread.
  func purgeImagesOlderThan(thresholdDate: Date, liveSnapshotIDs: [SnapshotIDWrapper]) {
    var filesToKeep: Set<String> = []
    for snapshotID in liveSnapshotIDs {
      filesToKeep.insert(
        imageFileName(snapshotID: snapshotID, imageType: ImageType.kImageTypeColor))
    }

    backgroundTaskGroup.enter()
    backgroundTaskQueue.async(group: backgroundTaskGroup) { [self, filesToKeep] in
      guard
        let enumerator = FileManager.default.enumerator(
          at: storageDirectory,
          includingPropertiesForKeys: [URLResourceKey.attributeModificationDateKey])
      else {
        backgroundTaskGroup.leave()
        return
      }

      for case let fileUrl as URL in enumerator {
        guard fileUrl.pathExtension == "jpg",
          !filesToKeep.contains(fileUrl.lastPathComponent)
        else {
          continue
        }

        do {
          let resourceValues = try fileUrl.resourceValues(forKeys: [
            URLResourceKey.attributeModificationDateKey
          ])

          guard let date = resourceValues.attributeModificationDate,
            date <= thresholdDate
          else {
            continue
          }

          try FileManager.default.removeItem(at: fileUrl)
        } catch {
          print("Failed to clean up images that are older than the threshold: \(error)")
        }
      }
      backgroundTaskGroup.leave()
    }
  }

  // Moves the image in disk from `oldPath` to `newPath`
  func copyImage(oldPath: URL, newPath: URL) {
    backgroundTaskGroup.enter()
    backgroundTaskQueue.async(group: backgroundTaskGroup) { [weak self] in
      guard let self = self else { return }
      // Copy a file only it's necessary.
      guard FileManager.default.fileExists(atPath: oldPath.path),
        !FileManager.default.fileExists(atPath: newPath.path)
      else {
        backgroundTaskGroup.leave()
        return
      }

      do {
        try FileManager.default.copyItem(at: oldPath, to: newPath)
      } catch {
        print("Failed to copy a file: \(error)")
      }
      backgroundTaskGroup.leave()
    }
  }

  // Returns the file path of the image for `snapshotID`.
  func imagePath(snapshotID: SnapshotIDWrapper) -> URL? {
    imagePath(snapshotID: snapshotID, imageType: ImageType.kImageTypeColor)
  }

  // Returns the file path of the image for `snapshotID`.
  // TODO(crbug.com/40942167): Remove this when the storage for all users has been
  // migrated.
  func legacyImagePath(snapshotID: String) -> URL? {
    legacyImagePath(snapshotID: snapshotID, imageType: ImageType.kImageTypeColor)
  }

  // Creates a directory that stores images.
  private func createStorageDirectory(directory: URL) {
    backgroundTaskGroup.enter()
    backgroundTaskQueue.async(group: backgroundTaskGroup) { [weak self] in
      guard let self = self else { return }
      do {
        try FileManager.default.createDirectory(
          at: directory, withIntermediateDirectories: true)
      } catch {
        print("Failed to create a snapshot storage: \(error)")
      }

      backgroundTaskGroup.leave()
    }
  }

  // Frees up disk by deleting all grey snapshots if they exist in `directory` because grey
  // snapshots are not stored anymore when `kGreySnapshotOptimization` feature is enabled.
  // TODO(crbug.com/40279302): This function should be removed in a few milestones
  // after `kGreySnapshotOptimization` feature is enabled by default.
  private func deleteAllGreyImages(directory: URL) {
    backgroundTaskGroup.enter()
    backgroundTaskQueue.async(group: backgroundTaskGroup) { [weak self] in
      guard let self = self else { return }
      guard FileManager.default.fileExists(atPath: storageDirectory.path) else {
        backgroundTaskGroup.leave()
        return
      }

      guard
        let enumerator = FileManager.default.enumerator(
          at: storageDirectory, includingPropertiesForKeys: nil)
      else {
        backgroundTaskGroup.leave()
        return
      }
      for case let fileUrl as URL in enumerator {
        do {
          if fileUrl.absoluteString.contains(
            suffixForImageType(imageType: ImageType.kImageTypeGrey))
          {
            try FileManager.default.removeItem(at: fileUrl)
          }
        } catch {
          print("Failed to delete all grey images: \(error)")
        }
      }
      backgroundTaskGroup.leave()
    }
  }

  // Returns the path of the image for `snapshotID` of type `imageType`.
  private func imagePath(snapshotID: SnapshotIDWrapper, imageType: ImageType) -> URL? {
    let fileName = imageFileName(snapshotID: snapshotID, imageType: imageType)
    if #available(iOS 16, *) {
      return storageDirectory.appending(path: fileName)
    } else {
      return storageDirectory.appendingPathComponent(fileName)
    }
  }

  // Returns the file name of the image for `snapshotID` of type `imageType`.
  private func imageFileName(snapshotID: SnapshotIDWrapper, imageType: ImageType) -> String {
    return String(
      format: "%08d", snapshotID.identifier) + suffixForImageType(imageType: imageType)
      + suffixForImageScale(imageScale: imageScale) + ".jpg"
  }

  // Returns the legacy path of the image for `snapshotID` of type `imageType`.
  // TODO(crbug.com/40942167): Remove this when the storage for all users has been
  // migrated.
  private func legacyImagePath(snapshotID: String, imageType: ImageType) -> URL? {
    let path =
      snapshotID + suffixForImageType(imageType: imageType)
      + suffixForImageScale(imageScale: imageScale) + ".jpg"
    if #available(iOS 16, *) {
      return storageDirectory.appending(path: path)
    } else {
      return storageDirectory.appendingPathComponent(path)
    }
  }

  // Returns the suffix to append to image filename for `image_type`.
  private func suffixForImageType(imageType: ImageType) -> String {
    switch imageType {
    case .kImageTypeColor:
      return ""
    case .kImageTypeGrey:
      return "Grey"
    }
  }

  // Returns the suffix to append to image filename for `image_scale`.
  private func suffixForImageScale(imageScale: ImageScale) -> String {
    switch imageScale {
    case kImageScale1X:
      return ""
    case kImageScale2X:
      return "@2x"
    default:
      return ""
    }
  }
}
