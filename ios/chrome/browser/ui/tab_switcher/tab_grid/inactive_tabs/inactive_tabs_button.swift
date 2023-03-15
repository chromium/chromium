// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_common_ui_colors_swift

/// The button displaying info about the current inactive tabs.
struct InactiveTabsButton: View {
  private enum Dimensions {
    static let verticalPadding: CGFloat = 8
    static let horizontalPadding: CGFloat = 16
    static let cornerRadius: CGFloat = 10
  }
  class State: ObservableObject {
    @Published var action: (() -> Void)?
    @Published var inactivityThresholdDisplayString: String?
    @Published var count: Int?
  }
  @ObservedObject var state: State
  @Environment(\.sizeCategory) var sizeCategory

  var body: some View {
    Button {
      state.action?()
    } label: {
      Group {
        if sizeCategory < .accessibilityMedium {
          regularLayout()
        } else {
          xxlLayout()
        }
      }
      .padding([.top, .bottom], Dimensions.verticalPadding)
      .padding([.leading, .trailing], Dimensions.horizontalPadding)
      .background(Color.secondaryBackground)
      .cornerRadius(Dimensions.cornerRadius)
      .environment(\.colorScheme, .dark)
    }
  }

  /// MARK - Layouts

  /// Displays the elements mostly horizontally, for when the size category is small.
  @ViewBuilder
  private func regularLayout() -> some View {
    HStack {
      leadingTextVStack {
        title()
        subtitle()
      }
      Spacer()
      counter()
      disclosure()
    }
  }

  /// Displays the button elements mostly vertically, for when the size category is large.
  @ViewBuilder
  private func xxlLayout() -> some View {
    HStack {
      leadingTextVStack {
        title()
        counter()
        subtitle()
      }
      Spacer()
      disclosure()
    }
  }

  /// MARK - Elements

  /// Displays the button title.
  @ViewBuilder
  private func title() -> some View {
    Text(L10NUtils.string(forMessageId: IDS_IOS_INACTIVE_TABS_BUTTON_TITLE))
      .foregroundColor(.white)
  }

  /// Displays the button subtitle.
  @ViewBuilder
  private func subtitle() -> some View {
    if let inactivityThresholdDisplayString = state.inactivityThresholdDisplayString {
      Text(
        L10NUtils.formatString(
          forMessageId: IDS_IOS_INACTIVE_TABS_BUTTON_SUBTITLE,
          argument: inactivityThresholdDisplayString)
      )
      .font(.caption2)
      .foregroundColor(.textSecondary)
    }
  }

  /// Displays the tabs count.
  ///
  /// If the count is larger than 99, "99+" is displayed.
  /// If the count is not set, this returns nothing.
  @ViewBuilder
  private func counter() -> some View {
    if let count = state.count {
      Text(count > 99 ? "99+" : "\(count)")
        .foregroundColor(.textSecondary)
    }
  }

  /// Displays the disclosure indicator.
  @ViewBuilder
  private func disclosure() -> some View {
    if #available(iOS 16.0, *) {
      Image(systemName: "chevron.right")
        .foregroundColor(.textTertiary)
        .fontWeight(.semibold)
        .flipsForRightToLeftLayoutDirection(true)
    } else {
      // fontWeight is not available on Image. Wrap it in a Text.
      Text(Image(systemName: "chevron.right"))
        .foregroundColor(.textTertiary)
        .fontWeight(.semibold)
        .flipsForRightToLeftLayoutDirection(true)
    }
  }

  /// VStack displaying its contents from the leading edge. Textual content is also leading aligned
  /// when displayed on multiple lines.
  @ViewBuilder
  private func leadingTextVStack(@ViewBuilder content: () -> some View) -> some View {
    VStack(alignment: .leading) {
      content()
    }
    .multilineTextAlignment(.leading)
  }
}
