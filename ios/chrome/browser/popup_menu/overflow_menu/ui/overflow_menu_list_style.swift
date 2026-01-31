// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Collection of constants and styling for the overflow menu `List`.
struct OverflowMenuListStyle: ViewModifier {
  /// Height of the destination list
  static let destinationListHeight: CGFloat = 113

  /// Default height if no other header or footer. This spaces the sections
  /// out properly.
  static let headerFooterHeight: CGFloat = 20

  /// The minimum row height for any row in the list.
  static let minimumRowHeight: CGFloat = 48

  func body(content: Content) -> some View {
    content.listStyle(InsetGroupedListStyle())
      // Allow sections to have very small headers controlling section spacing.
      .environment(\.defaultMinListHeaderHeight, 0)
      .environment(\.defaultMinListRowHeight, Self.minimumRowHeight)
  }
}

extension View {
  func overflowMenuListStyle() -> some View {
    modifier(OverflowMenuListStyle())
  }
}
