// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

extension Color {
  static var overflowMenuSeparator: Color {
    let uiColor = UIColor { traitCollection in
      let color =
        traitCollection.userInterfaceStyle == .dark
        ? UIColor(named: kTertiaryBackgroundColor) : UIColor(named: kGrey200Color)
      return color ?? .separator
    }

    if #available(iOS 15, *) {
      return Color(uiColor: uiColor)
    } else {
      return Color(uiColor)
    }
  }
}
