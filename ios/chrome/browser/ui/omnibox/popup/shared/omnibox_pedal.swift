// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import UIKit

@objcMembers public class OmniboxPedalData: NSObject, OmniboxPedal {
  /// Pedal title, as seen in the pedal button/row.
  public let title: String
  /// Pedal subtitle, e.g. "Settings -> Passwords"
  public let subtitle: String
  /// Describes the action performed; can be used for voiceover.
  public let hint: String
  /// Name of the image in the bundle.
  public let imageName: String
  /// Whether the pedal is displayed from an incognito session.
  public let incognito: Bool
  /// Action to run when the pedal is executed.
  public let action: () -> Void
  /// Action type for metrics collection. Int-casted OmniboxPedalId
  public let type: Int

  public init(
    title: String, subtitle: String,
    accessibilityHint: String, imageName: String, type: Int, incognito: Bool,
    action: @escaping () -> Void
  ) {
    self.title = title
    self.subtitle = subtitle
    self.hint = accessibilityHint
    self.imageName = imageName
    self.type = type
    self.incognito = incognito
    self.action = action
  }
}

extension OmniboxPedalData: OmniboxIcon {

  public var iconType: OmniboxIconType {
    return .suggestionIcon
  }

  public var iconImage: UIImage? {
    // Dark mode is set explicitly if incognito is enabled.
    let userInterfaceStyle =
      UITraitCollection(userInterfaceStyle: incognito ? .dark : .unspecified)
    return UIImage(
      named: self.imageName, in: nil,
      compatibleWith: UITraitCollection(traitsFrom: [.current, userInterfaceStyle]))
  }

  public var imageURL: CrURL? { return nil }
  public var iconImageTintColor: UIColor? { return nil }
  public var backgroundImage: UIImage? { return nil }
  public var backgroundImageTintColor: UIColor? { return nil }
  public var overlayImage: UIImage? { return nil }
  public var overlayImageTintColor: UIColor? { return nil }

}
