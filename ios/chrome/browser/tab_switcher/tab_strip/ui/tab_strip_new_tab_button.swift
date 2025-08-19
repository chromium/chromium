// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit
import ios_chrome_browser_tab_switcher_tab_strip_ui_swift_constants

/// UIView that contains an `UIButton` that opens a new tab.
class TabStripNewTabButton: UIView {
  private let button: UIButton = UIButton(type: .custom)

  /// Delegate that informs the receiver of actions on the new tab button.
  public var delegate: TabStripNewTabButtonDelegate?

  /// View used for by the `layoutGuideCenter`.
  public var layoutGuideView: UIView { return button }

  /// `true` if the user is in incognito.
  public var isIncognito: Bool {
    didSet {
      self.updateAccessibilityIdentifier()
    }
  }

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
      button.topAnchor.constraint(
        equalTo: self.topAnchor, constant: TabStripConstants.NewTabButton.topInset),
      button.widthAnchor.constraint(equalToConstant: TabStripConstants.NewTabButton.diameter),
      button.heightAnchor.constraint(equalTo: button.widthAnchor),
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
    let symbolSize = TabStripConstants.NewTabButton.symbolBiggerPointSize
    let closeSymbol = DefaultSymbolWithPointSize(
      kPlusSymbol, symbolSize)

    var configuration = UIButton.Configuration.borderless()
    configuration.contentInsets = .zero
    configuration.image = closeSymbol
    configuration.baseForegroundColor = TabStripHelper.newTabButtonSymbolColor

    button.configuration = configuration
    button.contentMode = .center
    button.imageView?.contentMode = .center
    button.layer.cornerRadius = TabStripConstants.NewTabButton.diameter / 2.0
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
        ? IDS_IOS_TOOLBAR_OPEN_NEW_TAB_INCOGNITO
        : IDS_IOS_TOOLBAR_OPEN_NEW_TAB)
  }
}
