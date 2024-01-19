// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

/// UIView that contains an two separators for tab strip collection view.
class TabStripDecorationView: UIView {

  override init(frame: CGRect) {
    super.init(frame: frame)
    translatesAutoresizingMaskIntoConstraints = false

    let solidBackgroundView: UIView = createSolidBackgroundView()
    let smallSeparator: UIView = createSeparatorView()
    let regularSeparator: UIView = createSeparatorView()

    addSubview(solidBackgroundView)
    addSubview(smallSeparator)
    addSubview(regularSeparator)

    NSLayoutConstraint.activate([
      /// `view` constraints.
      self.heightAnchor.constraint(
        equalToConstant: TabStripConstants.SeparatorView.viewHeight),

      /// `smallSeparator` constraints.
      smallSeparator.leadingAnchor.constraint(
        equalTo: self.leadingAnchor,
        constant: TabStripConstants.SeparatorView.leadingInset),
      smallSeparator.widthAnchor.constraint(
        equalToConstant: TabStripConstants.SeparatorView.separatorWidth),
      smallSeparator.heightAnchor.constraint(
        equalToConstant: TabStripConstants.SeparatorView.smallSeparatorHeight),
      smallSeparator.centerYAnchor.constraint(equalTo: self.centerYAnchor),

      /// `regularSeparator` constraints.
      regularSeparator.leadingAnchor.constraint(
        equalTo: smallSeparator.trailingAnchor,
        constant: TabStripConstants.SeparatorView.horizontalInset),
      regularSeparator.trailingAnchor.constraint(
        equalTo: self.trailingAnchor),
      regularSeparator.widthAnchor.constraint(
        equalToConstant: TabStripConstants.SeparatorView.separatorWidth),
      regularSeparator.heightAnchor.constraint(
        equalToConstant: TabStripConstants.SeparatorView.reuglarSeparatorHeight),
      regularSeparator.centerYAnchor.constraint(equalTo: self.centerYAnchor),

      /// `solidBackgroundView` constraints.
      solidBackgroundView.leadingAnchor.constraint(equalTo: self.leadingAnchor),
      solidBackgroundView.trailingAnchor.constraint(equalTo: regularSeparator.leadingAnchor),
      solidBackgroundView.topAnchor.constraint(equalTo: regularSeparator.topAnchor),
      solidBackgroundView.bottomAnchor.constraint(equalTo: regularSeparator.bottomAnchor),
    ])

  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  // MARK: - Private

  // Returns a new separator view.
  func createSeparatorView() -> UIView {
    let separatorView = UIView()
    separatorView.backgroundColor = UIColor(named: kGrey400Color)
    separatorView.translatesAutoresizingMaskIntoConstraints = false
    separatorView.layer.cornerRadius = TabStripConstants.SeparatorView.separatorCornerRadius
    return separatorView
  }

  // Returns a new solid background view.
  func createSolidBackgroundView() -> UIView {
    let solidBackgroundView = UIView(frame: .zero)
    solidBackgroundView.backgroundColor = UIColor(named: kGrey200Color)
    solidBackgroundView.translatesAutoresizingMaskIntoConstraints = false
    return solidBackgroundView
  }
}
