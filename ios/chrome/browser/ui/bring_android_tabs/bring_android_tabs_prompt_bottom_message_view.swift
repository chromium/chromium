// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import ios_chrome_common_ui_colors_swift

private let kPromptInnerPaddingHorizontal: CGFloat = 12
private let kPromptInnerPaddingVertical: CGFloat = 8
private let kPromptOuterPaddingHorizontal: CGFloat = 6
private let kPromptOuterPaddingVertical: CGFloat = 8
private let kPromptCornerRadius: CGFloat = 20
private let kTextVerticalSpacing: CGFloat = 12
private let kTextMinimumScaleFactor: CGFloat = 0.5
private let kSeparatorHeight: CGFloat = 1

// The view object for "Bring Android Tabs" bottom message prompt.
struct BringAndroidTabsPromptBottomMessageView: View {
  // Number of tabs brought from Android.
  let tabsCount: Int
  // Provider object for the view.
  weak var provider: BringAndroidTabsPromptBottomMessageProvider?

  var body: some View {
    HStack(alignment: .top) {
      Image(systemName: kTabsSymbol)
        .imageScale(.large)
        .padding(.top, kTextVerticalSpacing)
      VStack(alignment: .leading, spacing: kTextVerticalSpacing) {
        HStack(alignment: .top) {
          VStack(
            alignment: .leading,
            spacing: kTextVerticalSpacing - kPromptInnerPaddingVertical
          ) {
            Text(
              L10nUtils.pluralString(
                messageId: IDS_IOS_BRING_ANDROID_TABS_PROMPT_TITLE,
                number: self.tabsCount)
            )
            .font(.body)
            .minimumScaleFactor(kTextMinimumScaleFactor)
            Text(
              L10nUtils.pluralString(
                messageId: IDS_IOS_BRING_ANDROID_TABS_PROMPT_SUBTITLE,
                number: self.tabsCount)
            )
            .font(.footnote)
            .foregroundColor(.textSecondary)
            .minimumScaleFactor(kTextMinimumScaleFactor)
          }
          .padding(.top, kTextVerticalSpacing - kPromptInnerPaddingVertical)
          Spacer()
          Button(action: self.close) {
            Image(systemName: kXMarkCircleFillSymbol)
              .foregroundColor(.textQuaternary)
          }
          .accessibilityIdentifier(
            kBringAndroidTabsPromptBottomMessageCloseButtonAXId)
        }
        .padding(0)
        Divider().overlay(Color.separator).frame(height: kSeparatorHeight)
        Button(action: self.review) {
          HStack {
            Text(
              L10nUtils.pluralString(
                messageId:
                  IDS_IOS_BRING_ANDROID_TABS_PROMPT_REVIEW_TABS_BUTTON_BOTTOM_MESSAGE,
                number: self.tabsCount)
            )
            .minimumScaleFactor(kTextMinimumScaleFactor)
            Spacer()
            Image(systemName: kChevronForwardSymbol)
              .foregroundColor(.textSecondary)
          }
        }
        .foregroundColor(.chromeBlue)
        .accessibilityIdentifier(
          kBringAndroidTabsPromptBottomMessageReviewButtonAXId)
      }
      .padding(.bottom, kTextVerticalSpacing)
    }
    .padding(.horizontal, kPromptInnerPaddingHorizontal)
    .padding(.vertical, kPromptInnerPaddingVertical)
    .background(Color.primaryBackground)
    .clipShape(RoundedRectangle(cornerRadius: kPromptCornerRadius))
    .environment(\.colorScheme, .dark)
    .onAppear { self.onAppear() }
    .padding(.horizontal, kPromptOuterPaddingHorizontal)
    .padding(.vertical, kPromptOuterPaddingVertical)
  }

  // MARK: - Action handlers.

  // Invoked when the view is displayed.
  func onAppear() {
    self.provider?.delegate?.bringAndroidTabsPromptViewControllerDidShow()
  }

  // Review button action handler.
  func review() {
    self.provider?.delegate?
      .bringAndroidTabsPromptViewControllerDidTapReviewButton()
    self.provider?.commandHandler?.reviewAllBringAndroidTabs()
  }

  // Close button action handler.
  func close() {
    self.provider?.delegate?.bringAndroidTabsPromptViewControllerDidDismiss(
      swiped: false)
    self.provider?.commandHandler?.dismissBringAndroidTabsPrompt()
  }
}
