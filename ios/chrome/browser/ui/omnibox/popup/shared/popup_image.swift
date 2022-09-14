// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI

/// Model object for SwiftUI to show popup images. The final image itself is
/// a composite of a background, a main image, and an overlay. Any of the parts
/// can be absent.
@objcMembers public class PopupImage: NSObject, ObservableObject {
  /// The underlying icon for this image.
  public let icon: OmniboxIcon

  /// The SwiftUI `Image` and tint color for the main static image.
  public var iconImage: Image? {
    icon.iconImage.map { image in Image(uiImage: image) }
  }
  public var iconImageTintColor: Color? {
    icon.iconImageTintColor.map { color in Color(color) }
  }

  /// The SwiftUI `Image` and tint color for the background image, if present.
  public var backgroundImage: Image? {
    icon.backgroundImage.map { image in Image(uiImage: image) }
  }
  public var backgroundImageTintColor: Color? {
    icon.backgroundImageTintColor.map { color in Color(color) }
  }

  /// The SwiftUI `Image` and tint color for the overlay image, if present.
  public var overlayImage: Image? {
    icon.overlayImage.map { image in Image(uiImage: image) }
  }
  public var overlayImageTintColor: Color? {
    icon.overlayImageTintColor.map { color in Color(color) }
  }

  /// The SwiftUI `Image` for a dynamic main image loaded from a URL.
  public var iconImageFromURL: Image? {
    iconUIImageFromURL.map { image in Image(uiImage: image) }
  }

  /// The loaded `UIImage` for any main image that must be dynamically loaded
  /// from a URL.
  @Published public var iconUIImageFromURL: UIImage?

  public init(icon: OmniboxIcon) {
    self.icon = icon
  }
}
