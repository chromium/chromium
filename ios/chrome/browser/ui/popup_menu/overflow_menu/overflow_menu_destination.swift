// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Represents a destination in the overflow menu.
@objcMembers public class OverflowMenuDestination: NSObject, ObservableObject {
  /// The user-visible name of the destination.
  public let name: String

  /// The name of the image used to load the image for SwiftUI.
  let imageName: String

  /// The SwiftUI `Image` for the destination icon.
  public var image: Image {
    return Image(imageName)
  }

  /// Whether or not the destination is enterprise disabled.
  @Published public var enterpriseDisabled: Bool

  public init(
    name: String,
    imageName: String,
    enterpriseDisabled: Bool
  ) {
    self.name = name
    self.imageName = imageName
    self.enterpriseDisabled = enterpriseDisabled
  }
}

// MARK: - Identifiable

extension OverflowMenuDestination: Identifiable {
  public var id: String {
    return name
  }
}
