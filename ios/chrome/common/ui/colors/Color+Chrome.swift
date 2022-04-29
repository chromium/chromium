// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

// Adds easy SwiftUI access to the Chrome color palette.
extension Color {
  /// The background color.
  public static var cr_background: Color {
    return Color(kBackgroundColor)
  }

  /// The primary text color.
  public static var cr_textPrimaryColor: Color {
    return Color(kTextPrimaryColor)
  }

  /// The tertiary background color
  public static var cr_tertiaryBackgroundColor: Color {
    return Color(kTertiaryBackgroundColor)
  }

  /// The primary grouped background color.
  public static var cr_groupedPrimaryBackground: Color {
    return Color(kGroupedPrimaryBackgroundColor)
  }

  /// The secondary grouped background color.
  public static var cr_groupedSecondaryBackground: Color {
    return Color(kGroupedSecondaryBackgroundColor)
  }

  /// The primary background color.
  public static var cr_primaryBackground: Color {
    return Color(kPrimaryBackgroundColor)
  }

  /// The secondary background color.
  public static var cr_secondaryBackground: Color {
    return Color(kSecondaryBackgroundColor)
  }

  /// The grey200 color.
  public static var cr_grey200: Color {
    return Color(kGrey200Color)
  }

  /// The grey300 color.
  public static var cr_grey300: Color {
    return Color(kGrey300Color)
  }

  /// The grey500 color
  public static var cr_grey500: Color {
    return Color(kGrey500Color)
  }

  /// The grey700 color
  public static var cr_grey700: Color {
    return Color(kGrey700Color)
  }

  /// The blue color.
  public static var cr_blue: Color {
    return Color(kBlueColor)
  }

  /// The blue500 color.
  public static var cr_blue500: Color {
    return Color(kBlue500Color)
  }

  /// The table row view highlight color.
  public static var cr_tableRowViewHighlight: Color {
    return Color(kTableViewRowHighlightColor)
  }

  /// The table view sseparator color.
  public static var cr_separator: Color {
    return Color(kSeparatorColor)
  }
}
