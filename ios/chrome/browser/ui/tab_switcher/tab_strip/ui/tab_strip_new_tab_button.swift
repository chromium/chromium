// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

/// An `UIButton` displayed in the tab strip that opens a new tab.
class TabStripNewTabButton: UIButton {

  override init(frame: CGRect) {
    super.init(frame: frame)

    configureButton()
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  // MARK: - Private

  /// Configures the `UIButton`.
  private func configureButton() {
    var config = UIButton.Configuration.borderless()
    config.imagePadding = TabStripConstants.NewTabButton.contentInset
    configuration = config

    let closeSymbol: UIImage = CustomSymbolWithPointSize(
      kPlusCircleFillSymbol, TabStripConstants.NewTabButton.symbolPointSize)
    setImage(closeSymbol, for: .normal)
    tintColor = UIColor.orange
    translatesAutoresizingMaskIntoConstraints = false
  }
}
