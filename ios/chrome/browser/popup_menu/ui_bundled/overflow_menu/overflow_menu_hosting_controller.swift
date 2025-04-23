// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import UIKit

// UIHostingController subclass for the overflow menu. Mostly used to set
// preferredContentSize in compact height environments.
class OverflowMenuHostingController<Content>: UIHostingController<Content> where Content: View {
  // This should be the width of the share sheet in compact height environments.
  let compactHeightSheetWidth: CGFloat = 568

  let uiConfiguration: OverflowMenuUIConfiguration

  init(rootView: Content, uiConfiguration: OverflowMenuUIConfiguration) {
    self.uiConfiguration = uiConfiguration
    super.init(rootView: rootView)
    if #available(iOS 17, *) {
      let sizeTraits: [UITrait] = [UITraitVerticalSizeClass.self, UITraitHorizontalSizeClass.self]
      self.registerForTraitChanges(sizeTraits, action: #selector(updateUIOnTraitChange))
    }
  }

  required init(coder aDecoder: NSCoder) {
    fatalError("Not using storyboards")
  }

  var compactHeightPreferredContentSize: CGSize {
    return CGSize(
      width: compactHeightSheetWidth, height: presentingViewController?.view.bounds.size.height ?? 0
    )
  }

  override func viewDidLoad() {
    super.viewDidLoad()

    // Only set the preferredContentSize in height == compact because otherwise
    // it overrides the default size of the menu on iPad.
    preferredContentSize =
      traitCollection.verticalSizeClass == .compact ? compactHeightPreferredContentSize : .zero

    uiConfiguration.presentingViewControllerHorizontalSizeClass =
      presentingViewController?.traitCollection.horizontalSizeClass == .regular
      ? .regular : .compact
    uiConfiguration.presentingViewControllerVerticalSizeClass =
      presentingViewController?.traitCollection.verticalSizeClass == .regular ? .regular : .compact
  }

  @available(iOS, deprecated: 16.0)
  override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    self.updateUIOnTraitChange()
  }

  // Updates the presented view controller's horizontal and vertical layout on UITrait changes.
  @objc func updateUIOnTraitChange() {
    // Only set the preferredContentSize in height == compact because otherwise
    // it overrides the default size of the menu on iPad.
    preferredContentSize =
      traitCollection.verticalSizeClass == .compact ? compactHeightPreferredContentSize : .zero

    uiConfiguration.presentingViewControllerHorizontalSizeClass =
      presentingViewController?.traitCollection.horizontalSizeClass == .regular
      ? .regular : .compact
    uiConfiguration.presentingViewControllerVerticalSizeClass =
      presentingViewController?.traitCollection.verticalSizeClass == .regular ? .regular : .compact
  }
}
