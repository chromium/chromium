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
@objcMembers public class ImageFileManager: NSObject {
  // Directory where the images are saved.
  private let storageDirectory: URL
  // Scale for snapshot images. It may be smaller than the screen scale in order
  // to save memory on some devices.
  private let imageScale: ImageScale
  // A queue to manage the execution of tasks. The "default" QoS (Quality of Service Classes) level
  // falls between user-initiated and utility.
  private let taskQueue: DispatchQueue
  // A group to monitor the progress of tasks.
  private let taskGroup: DispatchGroup

  // Designated initializer. `storageDirectoryUrl` is the file path where all images managed by this
  // ImageFileManager are stored. `storageDirectoryUrl` is not guaranteed to exist. The contents of
  // `storageDirectoryUrl` are entirely managed by this ImageFileManager.
  //
  // To support renaming the directory where the snapshots are stored, it is possible to pass a
  // non-empty path via `legacyDirectoryUrl`. If present, then it will be moved to
  // `storageDirectoryUrl`.
  //
  // TODO(crbug.com/1501850): Remove `legacyDirectoryUrl` when the storage for all users has been
  // migrated.
  init(storageDirectoryUrl: URL, legacyDirectoryUrl: URL) {
    self.storageDirectory = storageDirectoryUrl
    self.imageScale = SnapshotImageScale.imageScaleForDevice()
    self.taskGroup = DispatchGroup()
    self.taskQueue = DispatchQueue(label: "org.chromium.image_file_manager", qos: .default)
    super.init()

    createStorageDirectory(directory: storageDirectoryUrl, legacyDirectory: legacyDirectoryUrl)

    // TODO(crbug.com/40279302): Delete this logic after a few milestones.
    deleteAllGreyImages(directory: storageDirectoryUrl)
  }

  // Waits until all tasks are completed. This is used for tests.
  func waitForAllTasks() {
    taskGroup.wait()
  }

  // Reads a color image from disk.
  func readImage(snapshotID: SnapshotIDWrapper, completion: @escaping (UIImage?) -> Void) {
    guard
      let imagePath = imagePath(
        snapshotID: snapshotID, imageType: ImageType.kImageTypeColor)
    else {
      completion(nil)
      return
    }

    taskGroup.enter()
    taskQueue.async(group: taskGroup) { [self] in
      completion(UIImage(contentsOfFile: imagePath.path))
      taskGroup.leave()
    }
  }

  // Reads a grey image from disk.
  func readGreyImage(snapshotID: SnapshotIDWrapper, completion: @escaping (UIImage?) -> Void) {
    guard
      let imagePath = imagePath(snapshotID: snapshotID, imageType: ImageType.kImageTypeGrey)
    else {
      completion(nil)
      return
    }

    taskGroup.enter()
    taskQueue.async(group: taskGroup) { [self] in
      completion(UIImage(contentsOfFile: imagePath.path))
      taskGroup.leave()
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

    taskGroup.enter()
    taskQueue.async(group: taskGroup) { [self] in
      guard let data = image.jpegData(compressionQuality: kJPEGImageQuality) else {
        taskGroup.leave()
        return
      }
      do {
        try data.write(to: imagePath)

        // Encrypt the snapshot file (mostly for Incognito, but can't hurt to
        // always do it).
        try FileManager.default.setAttributes(
          [
            .protectionKey: FileProtectionType.completeUntilFirstUserAuthentication
          ], ofItemAtPath: imagePath.path)
      } catch {
        print("Failed to store an image: \(error)")
      }
      taskGroup.leave()
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

    taskGroup.enter()
    taskQueue.async(group: taskGroup) { [self] in
      do {
        if FileManager.default.fileExists(atPath: imagePath.path) {
          try FileManager.default.removeItem(at: imagePath)
        }
      } catch {
        print("Failed to delete an image: \(error)")
      }
      taskGroup.leave()
    }
  }

  // Removes all images from disk.
  func removeAllImages() {
    taskGroup.enter()
    taskQueue.async(group: taskGroup) { [self] in
      do {
        // Delete the directory storing all images and create a brand new directory with the same
        // storage path.
        try FileManager.default.removeItem(at: storageDirectory)
        try FileManager.default.createDirectory(
          at: storageDirectory, withIntermediateDirectories: true)
      } catch {
        print("Failed to delete an image: \(error)")
      }
      taskGroup.leave()
    }
  }

  // Purges the storage of snapshots that are older than `date`. The snapshots for `liveSnapshotIDs`
  // will be kept. This will be done asynchronously on a background thread.
  func purgeImagesOlderThan(thresholdDate: Date, liveSnapshotIDs: [SnapshotIDWrapper]) {
    var filesToKeep: Set<URL> = []
    for snapshotID in liveSnapshotIDs {
      if let path = imagePath(snapshotID: snapshotID, imageType: ImageType.kImageTypeColor) {
        filesToKeep.insert(path)
      }
    }

    taskGroup.enter()
    taskQueue.async(group: taskGroup) { [self, filesToKeep] in
      guard
        let enumerator = FileManager.default.enumerator(
          at: storageDirectory,
          includingPropertiesForKeys: [URLResourceKey.attributeModificationDateKey])
      else {
        taskGroup.leave()
        return
      }

      for case let fileUrl as URL in enumerator {
        guard fileUrl.pathExtension == "jpg",
          !filesToKeep.contains(fileUrl)
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
      taskGroup.leave()
    }
  }

  // Renames snapshots with names in `oldIDs` to names in `newIDs`. It is a programmatic error if
  // the two array do not have the same length.
  func renameSnapshots(oldIDs: [String], newIDs: [SnapshotIDWrapper]) {
    assert(
      oldIDs.count == newIDs.count, "The number of old snapshot IDs and new IDs should be same")

    for (oldID, newID) in zip(oldIDs, newIDs) {
      renameSnapshot(oldID: oldID, newID: newID, imageType: ImageType.kImageTypeColor)
    }
  }

  // Moves the image in disk from `oldPath` to `newPath`
  func copyImage(oldPath: URL, newPath: URL) {
    taskGroup.enter()
    taskQueue.async(group: taskGroup) { [self] in
      // Copy a file only it's necessary.
      guard FileManager.default.fileExists(atPath: oldPath.path),
        !FileManager.default.fileExists(atPath: newPath.path)
      else {
        taskGroup.leave()
        return
      }

      do {
        try FileManager.default.copyItem(at: oldPath, to: newPath)
      } catch {
        print("Failed to copy a file: \(error)")
      }
      taskGroup.leave()
    }
  }

  // Returns the file path of the image for `snapshotID`.
  func imagePath(snapshotID: SnapshotIDWrapper) -> URL? {
    imagePath(snapshotID: snapshotID, imageType: ImageType.kImageTypeColor)
  }

  // Returns the file path of the image for `snapshotID`.
  // TODO(crbug.com/1501850): Remove this when the storage for all users has been
  // migrated.
  func legacyImagePath(snapshotID: String) -> URL? {
    legacyImagePath(snapshotID: snapshotID, imageType: ImageType.kImageTypeColor)
  }

  // Creates a directory that stores images and moves images from `legacyDirectory` to
  // `directory`.
  private func createStorageDirectory(directory: URL, legacyDirectory: URL) {
    taskGroup.enter()
    taskQueue.async(group: taskGroup) { [self] in
      do {
        try FileManager.default.createDirectory(
          at: directory, withIntermediateDirectories: true)
      } catch {
        print("Failed to create a snapshot storage: \(error)")
        taskGroup.leave()
        return
      }

      guard FileManager.default.fileExists(atPath: legacyDirectory.path, isDirectory: nil) else {
        taskGroup.leave()
        return
      }

      guard
        let enumerator = FileManager.default.enumerator(
          at: storageDirectory, includingPropertiesForKeys: nil)
      else {
        taskGroup.leave()
        return
      }
      for case let fileUrl as URL in enumerator {
        do {
          try FileManager.default.moveItem(
            at: fileUrl, to: directory)
        } catch {
          print("Failed to move images from a legacy path to a new path: \(error)")
        }
      }
      taskGroup.leave()
    }
  }

  // Frees up disk by deleting all grey snapshots if they exist in `directory` because grey
  // snapshots are not stored anymore when `kGreySnapshotOptimization` feature is enabled.
  // TODO(crbug.com/40279302): This function should be removed in a few milestones
  // after `kGreySnapshotOptimization` feature is enabled by default.
  private func deleteAllGreyImages(directory: URL) {
    taskGroup.enter()
    taskQueue.async(group: taskGroup) { [self] in
      guard FileManager.default.fileExists(atPath: storageDirectory.path) else {
        taskGroup.leave()
        return
      }

      guard
        let enumerator = FileManager.default.enumerator(
          at: storageDirectory, includingPropertiesForKeys: nil)
      else {
        taskGroup.leave()
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
      taskGroup.leave()
    }
  }

  // Renames the legacy path to the new path with `newID` and `imageType`.
  private func renameSnapshot(oldID: String, newID: SnapshotIDWrapper, imageType: ImageType) {
    guard let oldImagePath = legacyImagePath(snapshotID: oldID, imageType: imageType) else {
      return
    }
    guard let newImagePath = imagePath(snapshotID: newID, imageType: imageType) else {
      return
    }

    taskGroup.enter()
    taskQueue.async(group: taskGroup) { [self] in
      do {
        // Rename a file only when it's necessary.
        if FileManager.default.fileExists(atPath: oldImagePath.path)
          && !FileManager.default.fileExists(atPath: newImagePath.path)
        {
          try FileManager.default.moveItem(at: oldImagePath, to: newImagePath)
        }
      } catch {
        print("Failed to rename a file: \(error)")
      }
      taskGroup.leave()
    }
  }

  // Returns the path of the image for `snapshotID` of type `imageType`.
  private func imagePath(snapshotID: SnapshotIDWrapper, imageType: ImageType) -> URL? {
    let path =
      String(
        format: "%08d", snapshotID.identifier) + suffixForImageType(imageType: imageType)
      + suffixForImageScale(imageScale: imageScale) + ".jpg"
    if #available(iOS 16, *) {
      return storageDirectory.appending(path: path)
    } else {
      return storageDirectory.appendingPathComponent(path)
    }
  }

  // Returns the legacy path of the image for `snapshotID` of type `imageType`.
  // TODO(crbug.com/1501850): Remove this when the storage for all users has been
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
