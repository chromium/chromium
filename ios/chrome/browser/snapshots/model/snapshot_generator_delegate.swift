// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

// Protocol for the SnapshotGenerator's delegate.
@objc protocol SnapshotGeneratorDelegate {
  // Returns whether it is possible to capture a snapshot for a web state provided by
  // `webStateInfo`.
  func canTakeSnapshot(webStateInfo: WebStateSnapshotInfo) -> Bool

  // Invoked before capturing a snapshot for a web state provided by `webStateInfo`. The delegate
  // can remove subviews from the hierarchy or take other actions to ensure the snapshot is
  // correclty captured.
  func willUpdateSnapshot(webStateInfo: WebStateSnapshotInfo)

  // Returns the edge insets to use to crop the snapshot for a web state provided by `webState`
  // during generation. If the snapshot should not be cropped, then UIEdgeInsetsZero can be
  // returned. The returned insets should be in the coordinate system of the view returned by
  // `baseView()`.
  func snapshotEdgeInsets(webStateInfo: WebStateSnapshotInfo) -> UIEdgeInsets

  // Returns the list of overlay views that should be rendered over the page when generating the
  // snapshot for a web state provided by `webStateInfo`. If no overlays should be rendered, the
  // list may be nil or empty. The order of views in the array will be the z order of their image in
  // the composed snapshot. A view at the end of the array will appear in front of a view at the
  // beginning.
  func snapshotOverlays(webStateInfo: WebStateSnapshotInfo) -> [UIView]

  // Returns the base view to be snapshotted.
  func baseView(webStateInfo: WebStateSnapshotInfo) -> UIView?
}
