// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_common_ui_colors_swift

/// PreferenceKey to listen to changes of a view's size.
struct PopupMatchRowSizePreferenceKey: PreferenceKey {
  static var defaultValue = CGSize.zero
  // This function determines how to combine the preference values for two
  // child views. In the absence of any better combination method, just use the
  // second value.
  static func reduce(value: inout CGSize, nextValue: () -> CGSize) {
    value = nextValue()
  }
}

struct PopupMatchRowView: View {
  enum Colors {
    static let highlightingColor = Color(
      red: 26 / 255, green: 115 / 255, blue: 232 / 255, opacity: 1)
    static let highlightingGradient = Gradient(colors: [
      highlightingColor.opacity(0.85), highlightingColor,
    ])
  }

  enum Dimensions {
    static let actionButtonOffset = CGSize(width: -5, height: 0)
    static let actionButtonOuterPadding = EdgeInsets(top: 2, leading: 0, bottom: 2, trailing: 0)
    static let minHeight: CGFloat = 58

    enum VariationOne {
      static let padding = EdgeInsets(top: 9, leading: 0, bottom: 9, trailing: 0)
    }

    enum VariationTwo {
      static let padding = EdgeInsets(top: 9, leading: 0, bottom: 9, trailing: 10)
    }
  }

  @Environment(\.popupUIVariation) var uiVariation: PopupUIVariation
  @Environment(\.popupPasteButtonVariation) var pasteButtonVariation: PopupPasteButtonVariation
  @Environment(\.horizontalSizeClass) var sizeClass

  let match: PopupMatch
  let isHighlighted: Bool
  let toolbarConfiguration: ToolbarConfiguration
  let selectionHandler: () -> Void
  let trailingButtonHandler: () -> Void
  let uiConfiguration: PopupUIConfiguration

  @State var isPressed = false
  @State var childView = CGSize.zero
  @State var currentSize = CGSize.zero

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

  /// Enable this to tell the row it should display its own custom separator at the bottom.
  let shouldDisplayCustomSeparator: Bool
  var customSeparatorColor: Color {
    uiVariation == .one ? .separator : .grey200
  }
  @ViewBuilder
  var customSeparator: some View {
    HStack(spacing: 0) {
      Spacer().frame(
        width: leadingMarginForRowContent + spaceBetweenRowContentLeadingEdgeAndSuggestionText)
      customSeparatorColor.frame(height: 0.5)
    }
  }

  @Environment(\.layoutDirection) var layoutDirection: LayoutDirection

  var leadingMarginForRowContent: CGFloat {
    switch uiVariation {
    case .one:
      return uiConfiguration.omniboxLeadingSpace + 7
    case .two:
      return 8
    }
  }

  var trailingMarginForRowContent: CGFloat {
    switch uiVariation {
    case .one:
      return uiConfiguration.safeAreaTrailingSpace + kExpandedLocationBarLeadingMarginRefreshedPopup
    case .two:
      return 0
    }
  }

  var spaceBetweenRowContentLeadingEdgeAndCenterOfSuggestionImage: CGFloat {
    switch uiVariation {
    case .one:
      return uiConfiguration.omniboxLeadingImageLeadingSpace
    case .two:
      return 30
    }
  }

  var spaceBetweenTextAndImage: CGFloat {
    switch uiVariation {
    case .one:
      return 14
    case .two:
      return 15
    }
  }

  var spaceBetweenRowContentLeadingEdgeAndSuggestionText: CGFloat {
    switch uiVariation {
    case .one:
      return uiConfiguration.omniboxTextFieldLeadingSpace
    case .two:
      return 59
    }
  }

  var body: some View {
    ZStack {
      // This hides system separators when disabling them is not possible.
      backgroundColor.notifyOnSizeChange { size in
        currentSize = size
      }

      if shouldDisplayCustomSeparator {
        VStack {
          Spacer()
          customSeparator
        }
      }

      if isHighlighted {
        LinearGradient(gradient: Colors.highlightingGradient, startPoint: .top, endPoint: .bottom)
      } else if self.isPressed {
        Color.updatedTertiaryBackground
      }

      button

      let highlightColor = isHighlighted ? Color.white : nil

      // The content is in front of the button, for proper hit testing.
      HStack(alignment: .center, spacing: 0) {
        Color.clear.frame(width: leadingMarginForRowContent)

        match.image
          .map { image in
            PopupMatchImageView(
              image: image, highlightColor: highlightColor
            )
            .accessibilityHidden(true)
            .clipShape(RoundedRectangle(cornerRadius: 7, style: .continuous))
          }

        Color.clear.frame(width: spaceBetweenTextAndImage)

        VStack(alignment: .leading, spacing: 0) {
          VStack(alignment: .leading, spacing: 0) {
            GradientTextView(match.text, highlightColor: highlightColor)
              .lineLimit(1)
              .accessibilityHidden(true)

            if let subtitle = match.detailText, !subtitle.string.isEmpty {
              if match.hasAnswer {
                OmniboxText(subtitle, highlightColor: highlightColor)
                  .font(.footnote)
                  .lineLimit(match.numberOfLines)
                  .accessibilityHidden(true)
              } else {
                GradientTextView(
                  subtitle, highlightColor: highlightColor
                )
                .font(.footnote)
                .lineLimit(1)
                .accessibilityHidden(true)
              }
            }
          }
          .allowsHitTesting(false)
        }
        Spacer(minLength: 0)
        if match.isAppendable || match.isTabMatch {
          PopupMatchTrailingButton(match: match, action: trailingButtonHandler)
            .foregroundColor(isHighlighted ? highlightColor : .chromeBlue)
        } else if match.isClipboardMatch {
          // Clipboard matches are never appendable or tab matches.
          #if __IPHONE_16_0
            if #available(iOS 16.0, *) {
              let pasteButton: PasteButton = PasteButton(
                // The clipboard suggestion row is only going to appear for these
                // types of clipboard content.
                supportedContentTypes: [.text, .image, .url],
                payloadAction: { _ in
                  DispatchQueue.main.async {
                    // The clipboard content will be retrieved in a later stage by the
                    // clipboard provider. After the tap on `PasteButton`, later request
                    // on the same clipboard content won't trigger the permission popup.
                    selectionHandler()
                  }
                }
              )
              switch pasteButtonVariation {
              case .icon:
                pasteButton
                  .labelStyle(.iconOnly)
                  .buttonBorderShape(.capsule)
              case .iconText:
                pasteButton
                  .labelStyle(.titleAndIcon)
                  .buttonBorderShape(.capsule)
              }
            }
          #endif  // __IPHONE_16_0
        }
        Color.clear.frame(width: trailingMarginForRowContent)
      }
      .padding(
        uiVariation == .one ? Dimensions.VariationOne.padding : Dimensions.VariationTwo.padding
      )
      .environment(\.layoutDirection, layoutDirection)
    }
    .frame(maxWidth: .infinity, minHeight: Dimensions.minHeight)
    .preference(key: PopupMatchRowSizePreferenceKey.self, value: self.currentSize)
  }

  var backgroundColor: Color {
    switch uiVariation {
    case .one:
      return Color(toolbarConfiguration.backgroundColor)
    case .two:
      return .groupedSecondaryBackground
    }
  }
}

struct PopupMatchRowView_Previews: PreviewProvider {
  static var previews: some View = PopupView_Previews.previews
}
