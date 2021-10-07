// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Represents an action in the overflow menu.
@objcMembers public class OverflowMenuAction: NSObject, ObservableObject {
  /// The user-visible name of the action.
  public let name: String

  /// The name of the image used to load the image for SwiftUI.
  public let imageName: String

  /// The SwiftUI `Image` for the action icon.
  public var image: Image {
    return Image(imageName)
  }

  /// Whether thte action is dsabled by enterprise policy.
  @Published public var enterpriseDisabled: Bool

  /// Whether the action is disabled for non-enterprise reasons.
  @Published public var disabled = false

  public init(name: String, imageName: String, enterpriseDisabled: Bool) {
    self.name = name
    self.imageName = imageName
    self.enterpriseDisabled = enterpriseDisabled
  }
}

// MARK: - Identifiable

extension OverflowMenuAction: Identifiable {
  public var id: String {
    return name
  }
}
