// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

// Adds easy SwiftUI access to the Chrome color palette.
extension Color {
  /// The background color.
  public static var background: Color {
    return Color(kBackgroundColor)
  }

  /// The primary text color.
  public static var textPrimary: Color {
    return Color(kTextPrimaryColor)
  }

  /// The secondary text color.
  public static var textSecondary: Color {
    return Color(kTextSecondaryColor)
  }

  /// The tertiary text color.
  public static var textTertiary: Color {
    return Color(kTextTertiaryColor)
  }

  /// The quaternary text color.
  public static var textQuaternary: Color {
    return Color(kTextQuaternaryColor)
  }

  /// The tertiary background color
  public static var tertiaryBackground: Color {
    return Color(kTertiaryBackgroundColor)
  }

  /// The updated tertiary background color
  public static var updatedTertiaryBackground: Color {
    return Color(kUpdatedTertiaryBackgroundColor)
  }

  /// The primary grouped background color.
  public static var groupedPrimaryBackground: Color {
    return Color(kGroupedPrimaryBackgroundColor)
  }

  /// The secondary grouped background color.
  public static var groupedSecondaryBackground: Color {
    return Color(kGroupedSecondaryBackgroundColor)
  }

  /// The primary background color.
  public static var primaryBackground: Color {
    return Color(kPrimaryBackgroundColor)
  }

  /// The secondary background color.
  public static var secondaryBackground: Color {
    return Color(kSecondaryBackgroundColor)
  }

  /// The grey200 color.
  public static var grey200: Color {
    return Color(kGrey200Color)
  }

  /// The grey300 color.
  public static var grey300: Color {
    return Color(kGrey300Color)
  }

  /// The grey500 color
  public static var grey500: Color {
    return Color(kGrey500Color)
  }

  /// The grey700 color
  public static var grey700: Color {
    return Color(kGrey700Color)
  }

  /// The blue color.
  public static var chromeBlue: Color {
    return Color(kBlueColor)
  }

  /// The blue halo color.
  public static var blueHalo: Color {
    return Color(kBlueHaloColor)
  }

  /// The blue500 color.
  public static var blue500: Color {
    return Color(kBlue500Color)
  }

  /// The blue600 color.
  public static var blue600: Color {
    return Color(kBlue600Color)
  }

  /// The red500 color.
  public static var red500: Color {
    return Color(kRed500Color)
  }

  /// The table row view highlight color.
  public static var tableRowViewHighlight: Color {
    return Color(kTableViewRowHighlightColor)
  }

  /// The table view separator color.
  public static var separator: Color {
    return Color(kSeparatorColor)
  }

  /// The toolbar shadow color.
  public static var toolbarShadow: Color {
    return Color(kToolbarShadowColor)
  }
}
