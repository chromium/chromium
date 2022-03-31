// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Enum for which variant of the UI to use.
@objc public enum PopupUIVariation: Int {
  case one
  case two
}

private struct PopupUIVariationKey: EnvironmentKey {
  static let defaultValue: PopupUIVariation = .two
}

extension EnvironmentValues {
  var popupUIVariation: PopupUIVariation {
    get { self[PopupUIVariationKey.self] }
    set { self[PopupUIVariationKey.self] = newValue }
  }
}
