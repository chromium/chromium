// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Enum for which variant of the UI to use.
@objc public enum PopupUIVariation: Int {
  case one
  case two
}

/// Enum for which variant of the paste button UI to use.
@objc public enum PopupPasteButtonVariation: Int {
  case icon  //< Corresponds to LabelStyle.iconOnly
  case iconText  //< Corresponds to LabelStyle.titleAndIcon
}

private struct PopupUIVariationKey: EnvironmentKey {
  static let defaultValue: PopupUIVariation = .two
}

private struct PopupPasteButtonVariationKey: EnvironmentKey {
  static let defaultValue: PopupPasteButtonVariation = .icon
}

extension EnvironmentValues {
  var popupUIVariation: PopupUIVariation {
    get { self[PopupUIVariationKey.self] }
    set { self[PopupUIVariationKey.self] = newValue }
  }

  var popupPasteButtonVariation: PopupPasteButtonVariation {
    get { self[PopupPasteButtonVariationKey.self] }
    set { self[PopupPasteButtonVariationKey.self] = newValue }
  }
}
