// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

/// UIView that contains an separator for tab strip collection view..
class TabStripSeparatorView: UIView {

  override init(frame: CGRect) {
    super.init(frame: frame)
    translatesAutoresizingMaskIntoConstraints = false

    let separator: UIView = createSeparatorView()
    addSubview(separator)

    NSLayoutConstraint.activate([
      self.heightAnchor.constraint(
        equalToConstant: TabStripConstants.SeparatorView.viewHeight),

      separator.leadingAnchor.constraint(
        equalTo: self.leadingAnchor, constant: TabStripConstants.SeparatorView.leadingInset),
      separator.trailingAnchor.constraint(
        equalTo: self.trailingAnchor,
        constant: -TabStripConstants.SeparatorView.trailingInset),
      separator.widthAnchor.constraint(
        equalToConstant: TabStripConstants.SeparatorView.separatorWidth),
      separator.heightAnchor.constraint(
        equalToConstant: TabStripConstants.SeparatorView.separatorHeight),
      separator.centerYAnchor.constraint(equalTo: self.centerYAnchor),
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
}
