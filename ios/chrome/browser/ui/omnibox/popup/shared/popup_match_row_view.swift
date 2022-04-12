// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_common_ui_colors_swift

struct PopupMatchRowView: View {

  enum Dimensions {
    static let actionButtonOffset = CGSize(width: -5, height: 0)
    static let actionButtonOuterPadding = EdgeInsets(top: 2, leading: 0, bottom: 2, trailing: 0)
    static let leadingSpacing: CGFloat = 60
    static let minHeight: CGFloat = 58
    static let maxHeight: CGFloat = 98
    static let padding = EdgeInsets(top: 9, leading: 0, bottom: 9, trailing: 16)
    static let textHeight: CGFloat = 40
  }

  let match: PopupMatch
  let isHighlighted: Bool
  let selectionHandler: () -> Void
  let trailingButtonHandler: () -> Void

  @State var isPressed = false
  @State var childView = CGSize.zero

  var button: some View {

    let button =
      Button(action: selectionHandler) {
        Color.clear.contentShape(Rectangle())
      }
      .buttonStyle(PressedPreferenceKeyButtonStyle())
      .onPreferenceChange(PressedPreferenceKey.self) { isPressed in
        self.isPressed = isPressed
      }
      .accessibilityElement()
      .accessibilityLabel(match.text.string)
      .accessibilityValue(match.detailText?.string ?? "")
      .accessibilityRemoveTraits(.isButton)

    if match.isAppendable || match.isTabMatch {
      let trailingActionAccessibilityTitle =
        match.isTabMatch
        ? L10NUtils.string(forMessageId: IDS_IOS_OMNIBOX_POPUP_SWITCH_TO_OPEN_TAB)
        : L10NUtils.string(forMessageId: IDS_IOS_OMNIBOX_POPUP_APPEND)

      return
        button
        .accessibilityAction(
          named: trailingActionAccessibilityTitle!, trailingButtonHandler
        )
    } else {
      return button
    }
  }

  var body: some View {
    ZStack {
      // TODO(crbug.com/1311615): This next line should be `backgroundColor`,
      // but for some reason, that causes the tests to fail on the bots only.
      if self.isPressed || self.isHighlighted { Color.cr_tableRowViewHighlight }

      button

      // The content is in front of the button, for proper hit testing.
      HStack(alignment: .center, spacing: 0) {
        HStack(alignment: .center, spacing: 0) {
          Spacer()
          match.image.map { image in
            PopupMatchImageView(image: image)
              .accessibilityHidden(true)
          }
          Spacer()
        }.frame(width: Dimensions.leadingSpacing)
        VStack(alignment: .leading, spacing: 0) {
          VStack(alignment: .leading, spacing: 0) {
            OmniboxText(match.text)
              .lineLimit(1)
              .truncatedWithGradient(colored: backgroundColor)
              .accessibilityHidden(true)

            if let subtitle = match.detailText, !subtitle.string.isEmpty {
              OmniboxText(subtitle)
                .font(.footnote)
                .foregroundColor(Color.gray)
                .lineLimit(1)
                .truncatedWithGradient(colored: backgroundColor)
                .accessibilityHidden(true)
            }
          }
          .frame(height: Dimensions.textHeight)
          .allowsHitTesting(false)
        }
        Spacer()
        if match.isAppendable || match.isTabMatch {
          PopupMatchTrailingButton(match: match, action: trailingButtonHandler)
        }
      }
      .padding(Dimensions.padding)
    }
    .frame(maxWidth: .infinity, minHeight: Dimensions.minHeight, maxHeight: Dimensions.maxHeight)
  }

  var backgroundColor: Color {
    if self.isPressed || self.isHighlighted {
      return .cr_tableRowViewHighlight
    } else {
      return .cr_groupedSecondaryBackground
    }
  }
}

struct PopupMatchRowView_Previews: PreviewProvider {
  static var previews: some View = PopupView_Previews.previews
}
