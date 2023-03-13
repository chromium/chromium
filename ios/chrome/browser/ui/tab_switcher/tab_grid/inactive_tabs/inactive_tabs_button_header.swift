// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import UIKit

/// A collection view header containing the Inactive Tabs button displayed when
/// there are inactive tabs.
@objc
public class InactiveTabsButtonHeader: UICollectionReusableView {
  private enum Dimensions {
    /// The margin at the top of the header.
    static let topMargin: CGFloat = 12
    /// The margin on the other edges of the header.
    static let margin: CGFloat = 16
  }

  /// The state driving the SwiftUI button.
  private let buttonState = InactiveTabsButton.State()
  /// The hosting controller to map to the SwiftUI button.
  private lazy var hostingController = {
    UIHostingController(rootView: InactiveTabsButton(state: buttonState))
  }()

  /// Installs the hosting controller in the parent view controller.
  @objc public var parent: UIViewController? {
    didSet {
      guard hostingController.parent != parent else { return }

      if let parent {
        parent.addChild(hostingController)
        hostingController.view.backgroundColor = .clear
        hostingController.view.translatesAutoresizingMaskIntoConstraints = false
        addSubview(hostingController.view)
        hostingController.didMove(toParent: parent)
        NSLayoutConstraint.activate([
          hostingController.view.topAnchor.constraint(
            equalTo: topAnchor, constant: Dimensions.topMargin),
          hostingController.view.leadingAnchor.constraint(
            equalTo: leadingAnchor, constant: Dimensions.margin),
          hostingController.view.bottomAnchor.constraint(
            equalTo: bottomAnchor, constant: -Dimensions.margin),
          hostingController.view.trailingAnchor.constraint(
            equalTo: trailingAnchor, constant: -Dimensions.margin),
        ])
      } else {
        hostingController.willMove(toParent: nil)
        hostingController.view.removeFromSuperview()
        hostingController.removeFromParent()
      }
    }
  }

  /// The callback when the Inactive Tabs button is pressed.
  @objc public var buttonAction: (() -> Void)? {
    get { buttonState.action }
    set { buttonState.action = newValue }
  }

  /// The display string showing the number of days after which tabs are considered inactive.
  @objc public var inactivityThresholdDisplayString: String? {
    get { buttonState.inactivityThresholdDisplayString }
    set { buttonState.inactivityThresholdDisplayString = newValue }
  }

  /// Sets the count on the Inactive Tabs button.
  /// Note: this can't be a property because Swift doesn't support "Int?" with @objc.
  @objc public func configure(count: Int) {
    buttonState.count = count
  }
}
