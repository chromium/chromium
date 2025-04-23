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
  /// The image for the Pedal.
  public let image: UIImage
  public let imageTintColor: UIColor?
  public let backgroundColor: UIColor?
  public let imageBorderColor: UIColor?
  /// Action to run when the pedal is executed.
  public let action: () -> Void
  /// Action type for metrics collection. Int-casted OmniboxPedalId
  public let type: Int

  public init(
    title: String, subtitle: String,
    accessibilityHint: String,
    image: UIImage,
    imageTintColor: UIColor?,
    backgroundColor: UIColor?,
    imageBorderColor: UIColor?,
    type: Int,
    action: @escaping () -> Void
  ) {
    self.title = title
    self.subtitle = subtitle
    self.hint = accessibilityHint
    self.image = image
    self.imageTintColor = imageTintColor
    self.backgroundColor = backgroundColor
    self.imageBorderColor = imageBorderColor
    self.type = type
    self.action = action
  }
}

extension OmniboxPedalData: OmniboxIcon {

  public var iconType: OmniboxIconType {
    return .suggestionIcon
  }

  public var iconImage: UIImage? {
    return image
  }

  public var imageURL: CrURL? { return nil }

  public var iconImageTintColor: UIColor? {
    return imageTintColor
  }

  public var backgroundImageTintColor: UIColor? {
    return backgroundColor
  }

  public var borderColor: UIColor? {
    return imageBorderColor
  }

}
