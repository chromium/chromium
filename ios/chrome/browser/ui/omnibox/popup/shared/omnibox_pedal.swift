// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import UIKit

@objcMembers public class OmniboxPedalData: NSObject, OmniboxPedal {
  public let hint: String
  public let imageName: String
  public let action: () -> Void

  public init(hint: String, imageName: String, action: @escaping () -> Void) {
    self.hint = hint
    self.imageName = imageName
    self.action = action
  }
}

extension OmniboxPedalData: OmniboxIcon {

  public var iconType: OmniboxIconType {
    return .suggestionIcon
  }

  public var iconImage: UIImage? {
    return UIImage(named: self.imageName)
  }

  public var imageURL: CrURL? { return nil }
  public var iconImageTintColor: UIColor? { return nil }
  public var backgroundImage: UIImage? { return nil }
  public var backgroundImageTintColor: UIColor? { return nil }
  public var overlayImage: UIImage? { return nil }
  public var overlayImageTintColor: UIColor? { return nil }

}
