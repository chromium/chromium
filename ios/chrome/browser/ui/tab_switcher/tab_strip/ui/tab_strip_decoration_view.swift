// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit
import ios_chrome_browser_ui_tab_switcher_tab_strip_ui_swift_constants

/// UIView that contains two decoration separators for tab strip collection
/// view. They are visible when the selected cell reaches an edge of the
/// collection view and if the collection view can be scrolled.
class TabStripDecorationView: UIView {

  override init(frame: CGRect) {
    super.init(frame: frame)
    translatesAutoresizingMaskIntoConstraints = false

    if TabStripFeaturesUtils.hasBlackBackground {
      overrideUserInterfaceStyle = .dark
    }

    let solidBackgroundView: UIView = createSolidBackgroundView()
    let smallSeparator: UIView = createSeparatorView()
    let regularSeparator: UIView = createSeparatorView()

    addSubview(solidBackgroundView)
    addSubview(smallSeparator)
    addSubview(regularSeparator)

    NSLayoutConstraint.activate([
      /// `view` constraints.
      self.heightAnchor.constraint(
        equalToConstant: TabStripConstants.StaticSeparator.viewHeight),

      /// `smallSeparator` constraints.
      smallSeparator.leadingAnchor.constraint(
        equalTo: self.leadingAnchor,
        constant: TabStripConstants.StaticSeparator.leadingInset),
      smallSeparator.widthAnchor.constraint(
        equalToConstant: TabStripConstants.StaticSeparator.separatorWidth),
      smallSeparator.heightAnchor.constraint(
        equalToConstant: TabStripConstants.StaticSeparator.smallSeparatorHeight),
      smallSeparator.centerYAnchor.constraint(equalTo: self.centerYAnchor),

      /// `regularSeparator` constraints.
      regularSeparator.leadingAnchor.constraint(
        equalTo: smallSeparator.trailingAnchor,
        constant: TabStripConstants.StaticSeparator.horizontalInset),
      regularSeparator.trailingAnchor.constraint(
        equalTo: self.trailingAnchor),
      regularSeparator.widthAnchor.constraint(
        equalToConstant: TabStripConstants.StaticSeparator.separatorWidth),
      regularSeparator.heightAnchor.constraint(
        equalToConstant: TabStripConstants.StaticSeparator.regularSeparatorHeight),
      regularSeparator.centerYAnchor.constraint(equalTo: self.centerYAnchor),

      /// `solidBackgroundView` constraints.
      solidBackgroundView.leadingAnchor.constraint(equalTo: self.leadingAnchor),
      solidBackgroundView.trailingAnchor.constraint(equalTo: regularSeparator.leadingAnchor),
      solidBackgroundView.topAnchor.constraint(
        equalTo: regularSeparator.topAnchor,
        constant: -TabStripConstants.StaticSeparator.solidBackgroundVerticalPadding),
      solidBackgroundView.bottomAnchor.constraint(
        equalTo: regularSeparator.bottomAnchor,
        constant: +TabStripConstants.StaticSeparator.solidBackgroundVerticalPadding),
    ])

  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  // MARK: - Private

  // Returns a new separator view.
  func createSeparatorView() -> UIView {
    let separatorView = UIView()
    separatorView.backgroundColor = UIColor(named: kTextQuaternaryColor)
    separatorView.translatesAutoresizingMaskIntoConstraints = false
    separatorView.layer.cornerRadius = TabStripConstants.StaticSeparator.separatorCornerRadius
    return separatorView
  }

  // Returns a new solid background view.
  func createSolidBackgroundView() -> UIView {
    let solidBackgroundView = UIView()
    solidBackgroundView.backgroundColor = TabStripHelper.backgroundColor
    solidBackgroundView.translatesAutoresizingMaskIntoConstraints = false
    return solidBackgroundView
  }
}
