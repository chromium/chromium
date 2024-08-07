// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_common_ui_colors_swift

/// The button displaying info about the current inactive tabs.
struct InactiveTabsButton: View {
  private enum Dimensions {
    static let verticalPadding: CGFloat = 10
    static let horizontalPadding: CGFloat = 16
    static let spacing: CGFloat = 3
    static let cornerRadius: CGFloat = 10
  }
  class State: ObservableObject {
    @Published var action: (() -> Void)?
    @Published var daysThreshold: Int?
    @Published var count: Int?
  }
  @ObservedObject var state: State
  @Environment(\.dynamicTypeSize) var typeSize

  var body: some View {
    Button {
      state.action?()
    } label: {
      if typeSize < .accessibility1 {
        regularLayout()
      } else {
        xxlLayout()
      }
    }
    .buttonStyle(InactiveTabsButtonStyle())
    .accessibilityIdentifier(kInactiveTabsButtonAccessibilityIdentifier)
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
    Text(titleString)
      .foregroundColor(.white)
  }

  /// Displays the button subtitle.
  @ViewBuilder
  private func subtitle() -> some View {
    if let subtitleString {
      Text(subtitleString)
        .font(.footnote)
        .foregroundColor(.textSecondary)
    }
  }

  /// Displays the tabs count.
  ///
  /// If the count is larger than 99, "99+" is displayed.
  /// If the count is not set, this returns nothing.
  @ViewBuilder
  private func counter() -> some View {
    if let counterString {
      Text(counterString)
        .foregroundColor(.textSecondary)
    }
  }

  /// Displays the disclosure indicator.
  @ViewBuilder
  private func disclosure() -> some View {
    Image(systemName: kChevronForwardSymbol)
      .foregroundColor(.textTertiary)
      .fontWeight(.semibold)
  }

  /// VStack displaying its contents from the leading edge. Textual content is also leading aligned
  /// when displayed on multiple lines.
  @ViewBuilder
  private func leadingTextVStack(@ViewBuilder content: () -> some View) -> some View {
    VStack(alignment: .leading, spacing: Dimensions.spacing) {
      content()
    }
    .multilineTextAlignment(.leading)
  }

  /// MARK - Styles

  /// Style for the main button, i.e. the top-level view.
  private struct InactiveTabsButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
      configuration.label
        .padding([.top, .bottom], Dimensions.verticalPadding)
        .padding([.leading, .trailing], Dimensions.horizontalPadding)
        .background(
          configuration.isPressed ? Color(.systemGray4) : Color.groupedSecondaryBackground
        )
        .cornerRadius(Dimensions.cornerRadius)
        .environment(\.colorScheme, .dark)
    }
  }

  /// MARK - Strings

  /// String for the title.
  private var titleString: String {
    L10nUtils.string(messageId: IDS_IOS_INACTIVE_TABS_BUTTON_TITLE)
  }

  /// Optional string for the subtitle.
  private var subtitleString: String? {
    if let daysThreshold = state.daysThreshold {
      return L10nUtils.formatString(
        messageId: IDS_IOS_INACTIVE_TABS_BUTTON_SUBTITLE,
        argument: String(daysThreshold))
    } else {
      return nil
    }
  }

  /// Optional string for the counter.
  private var counterString: String? {
    if let count = state.count {
      return count > 99 ? "99+" : "\(count)"
    } else {
      return nil
    }
  }
}
