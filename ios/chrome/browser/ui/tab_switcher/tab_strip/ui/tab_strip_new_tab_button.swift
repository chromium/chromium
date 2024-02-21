// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

/// UIView that contains an `UIButton` that opens a new tab.
class TabStripNewTabButton: UIView {

  /// Delegate that informs the receiver of actions on the new tab button.
  public var delegate: TabStripNewTabButtonDelegate?

  /// `true` if the user is in incognito.
  public var isIncognito: Bool {
    didSet {
      self.updateAccessibilityIdentifier()
    }
  }

  private let button: UIButton = UIButton(type: .custom)

  override init(frame: CGRect) {
    isIncognito = false

    super.init(frame: frame)
    translatesAutoresizingMaskIntoConstraints = false

    configureButton()
    addSubview(button)
    button.accessibilityIdentifier = TabStripConstants.NewTabButton.accessibilityIdentifier

    NSLayoutConstraint.activate([
      button.leadingAnchor.constraint(
        equalTo: self.leadingAnchor, constant: TabStripConstants.NewTabButton.leadingInset),
      button.trailingAnchor.constraint(
        equalTo: self.trailingAnchor, constant: -TabStripConstants.NewTabButton.trailingInset),
      button.topAnchor.constraint(
        equalTo: self.topAnchor, constant: TabStripConstants.NewTabButton.topInset),
      button.bottomAnchor.constraint(
        equalTo: self.bottomAnchor, constant: -TabStripConstants.NewTabButton.bottomInset),
    ])
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  // MARK: - Private

  /// Called when the `button` has been tapped.
  @objc func buttonTapped() {
    delegate?.newTabButtonTapped()
  }

  /// Configures the `UIButton`.
  private func configureButton() {
    let closeSymbol: UIImage = DefaultSymbolWithPointSize(
      kPlusSymbol, TabStripConstants.NewTabButton.symbolPointSize)

    var configuration = UIButton.Configuration.borderless()
    configuration.contentInsets = .zero

    configuration.image = closeSymbol
    configuration.baseForegroundColor = UIColor(named: kTextSecondaryColor)
    button.configuration = configuration
    button.imageView?.contentMode = .center
    button.layer.cornerRadius = TabStripConstants.NewTabButton.cornerRadius
    button.backgroundColor = UIColor(named: kGroupedSecondaryBackgroundColor)

    button.translatesAutoresizingMaskIntoConstraints = false
    button.addTarget(self, action: #selector(buttonTapped), for: .touchUpInside)
    button.isPointerInteractionEnabled = true
    if #available(iOS 17.0, *) {
      button.hoverStyle = .init(effect: .lift, shape: .circle)
    }
  }

  /// Updates the `accessibilityLabel` according to the current state of
  /// `isIncognito`.
  private func updateAccessibilityIdentifier() {
    button.accessibilityLabel = L10nUtils.stringWithFixup(
      messageId: isIncognito
        ? IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB
        : IDS_IOS_TOOLS_MENU_NEW_TAB)
  }
}
