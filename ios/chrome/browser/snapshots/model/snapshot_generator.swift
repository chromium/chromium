// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

// A class that generates snapshot images for the WebState associated with this class.
@objcMembers public class SnapshotGenerator: NSObject {
  // A wrapper class for the associated WebState.
  private let webStateInfo: WebStateSnapshotInfo
  // The SnapshotGenerator delegate to obtain the information about UIView.
  weak var delegate: SnapshotGeneratorDelegate?

  // Designated initializer.
  init(webStateInfo: WebStateSnapshotInfo) {
    self.webStateInfo = webStateInfo
    self.delegate = nil
  }

  // Generates a new snapshot and runs a callback with the new snapshot image. It uses UIKit-based
  // snapshot APIs if the web state is showing anything other than a web view (e.g., native
  // content). Otherwise, it uses WebKit-based snapshot APIs.
  func generateSnapshot(completion: ((UIImage?) -> Void)?) {
    guard let lastCommitedUrl = webStateInfo.lastCommittedURL() else {
      return
    }
    var scheme = ""
    if lastCommitedUrl.nsurl != nil {
      scheme = lastCommitedUrl.nsurl.scheme ?? ""
    }
    if scheme != "chrome" && webStateInfo.canTakeSnapshot() {
      // Take the snapshot using the optimized WKWebView snapshotting API for pages loaded in the
      // web view when the WebState snapshot API is available.
      generateWKWebViewSnapshot(completion: completion)
      return
    }
    // Use the UIKit-based snapshot API as a fallback when the WKWebView API is unavailable.
    let snapshot = generateUIViewSnapshotWithOverlays()
    if completion != nil {
      completion?(snapshot)
    }
  }

  // Generates and returns a new snapshot image with UIKit-based snapshot API.
  func generateUIViewSnapshot() -> UIImage? {
    if !canTakeSnapshot() {
      return nil
    }

    delegate!.willUpdateSnapshot(webStateInfo: webStateInfo)

    guard let baseView = delegate!.baseView(webStateInfo: webStateInfo) else {
      return nil
    }
    let baseViewInsets = delegate!.snapshotEdgeInsets(webStateInfo: webStateInfo)
    let frameInBaseView = baseView.bounds.inset(by: baseViewInsets)

    let baseImage = convertBaseView(baseView: baseView)
    return cropImage(baseImage: baseImage, frameInBaseView: frameInBaseView)
  }

  // Generates and returns a new snapshot image with UIKit-based snapshot API. The generated image
  // includes overlays (e.g., infobars, the download manager, and sad tab view).
  func generateUIViewSnapshotWithOverlays() -> UIImage? {
    return addOverlays(baseImage: generateUIViewSnapshot())
  }

  // Asynchronously generates a new snapshot with WebKit-based snapshot API and runs a callback with
  // the new snapshot image. It is an error to call this method if the web state is showing anything
  // other (e.g., native content) than a web view.
  private func generateWKWebViewSnapshot(completion: ((UIImage?) -> Void)?) {
    if !canTakeSnapshot() {
      completion?(nil)
      return
    }
    delegate!.willUpdateSnapshot(webStateInfo: webStateInfo)

    guard let baseView = delegate!.baseView(webStateInfo: webStateInfo) else {
      completion?(nil)
      return
    }
    let baseViewInsets = delegate!.snapshotEdgeInsets(webStateInfo: webStateInfo)
    let frameInBaseView = baseView.bounds.inset(by: baseViewInsets)

    let wrappedCompletion = { [weak self] (image: UIImage?) in
      let snapshot = self?.addOverlays(baseImage: image)
      completion?(snapshot)
    }
    webStateInfo.takeSnapshot(frameInBaseView, callback: wrappedCompletion)
  }

  // Converts an UIView to an UIImage. The size of generated UIImage is the same as `baseView`.
  private func convertBaseView(baseView: UIView) -> UIImage? {
    // Disable the automatic view dimming UIKit performs if a view is presented modally over
    // `baseView`.
    baseView.tintAdjustmentMode = .normal

    let format = UIGraphicsImageRendererFormat.preferred()
    format.scale = SnapshotImageScale.floatForDevice()
    format.opaque = true

    let renderer = UIGraphicsImageRenderer(bounds: baseView.bounds, format: format)

    var snapshotSuccess = true
    let image = renderer.image { (context) in
      // Render the view's layer via `render(in:)`.
      // To mitigate against crashes like crbug.com/1429512, ensure that the layer's position is
      // valid. If not, mark the snapshotting as failed.
      let layer = baseView.layer
      let pos = layer.position
      if pos.x.isNaN || pos.y.isNaN {
        snapshotSuccess = false
      } else {
        baseView.layer.render(in: context.cgContext)
      }
    }

    // Set the mode to UIViewTintAdjustmentModeAutomatic.
    baseView.tintAdjustmentMode = .automatic

    if snapshotSuccess {
      return image
    }
    return nil
  }

  // Crops an UIImage to `frameInBaseView`.
  private func cropImage(baseImage: UIImage?, frameInBaseView: CGRect) -> UIImage? {
    guard let baseImage = baseImage else {
      return nil
    }
    guard let cgImage = baseImage.cgImage else {
      return nil
    }
    let scale = baseImage.scale

    var frame = frameInBaseView
    frame.origin.x *= scale
    frame.origin.y *= scale
    frame.size.width *= scale
    frame.size.height *= scale
    let croppedCGImage = cgImage.cropping(to: frame)

    return UIImage(
      cgImage: croppedCGImage!, scale: baseImage.imageRendererFormat.scale,
      orientation: baseImage.imageOrientation)
  }

  // Returns false if WebState or the view is not ready for snapshot.
  private func canTakeSnapshot() -> Bool {
    // This allows for easier unit testing of classes that use SnapshotGenerator.
    if delegate == nil {
      return false
    }

    // Do not generate a snapshot if web usage is disabled (as the WebState's view is blank in that
    // case).
    if !webStateInfo.isWebUsageEnabled() {
      return false
    }

    return delegate!.canTakeSnapshot(webStateInfo: webStateInfo)
  }

  // Returns an image of the `baseImage` overlaid with overlays.
  private func addOverlays(baseImage: UIImage?) -> UIImage? {
    if delegate == nil {
      return nil
    }
    guard let baseImage = baseImage else {
      return nil
    }
    guard let baseView = delegate!.baseView(webStateInfo: webStateInfo) else {
      return nil
    }
    let baseViewInsets = delegate!.snapshotEdgeInsets(webStateInfo: webStateInfo)
    let frameInBaseView = baseView.bounds.inset(by: baseViewInsets)
    let snapshotFrameInWindow = baseView.convert(frameInBaseView, to: nil)

    let overlays = delegate!.snapshotOverlays(webStateInfo: webStateInfo)
    // Note: If the baseImage scale differs from device scale, the baseImage size may slightly
    // differ from `snapshotFrameInWindow` size due to rounding. Do not attempt to compare the
    // `baseImage` size and `snapshotFrameInWindow` size.
    if overlays.count == 0 {
      return baseImage
    }

    let format = UIGraphicsImageRendererFormat.preferred()
    format.scale = SnapshotImageScale.floatForDevice()
    format.opaque = true

    let renderer = UIGraphicsImageRenderer(size: snapshotFrameInWindow.size, format: format)

    return renderer.image { (context) in
      let cgContextRef = context.cgContext

      // The base image is already a cropped snapshot so it is drawn at the origin of the new image.
      baseImage.draw(in: CGRect(origin: CGPoint.zero, size: snapshotFrameInWindow.size))

      // This shifts the origin of the context so that future drawings can be in window coordinates.
      // For example, suppose that the desired snapshot area is at (0, 99) in the window coordinate
      // space. Drawing at (0, 99) will appear as (0, 0) in the resulting image.
      cgContextRef.translateBy(
        x: -snapshotFrameInWindow.origin.x, y: -snapshotFrameInWindow.origin.y)

      for overlay in overlays {
        cgContextRef.saveGState()
        let frameInWindow = baseView.convert(overlay.frame, to: nil)
        // This shifts the context so that drawing starts at the overlay's offset.
        cgContextRef.translateBy(x: frameInWindow.origin.x, y: frameInWindow.origin.y)
        overlay.layer.render(in: cgContextRef)
        cgContextRef.restoreGState()
      }
    }
  }
}
