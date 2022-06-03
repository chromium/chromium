// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

struct QuickActionsWidget: Widget {
  // Changing |kind| or deleting this widget will cause all installed instances of this widget to
  // stop updating and show the placeholder state.
  let kind: String = "QuickActionsWidget"

  var body: some WidgetConfiguration {
    StaticConfiguration(kind: kind, provider: Provider()) { entry in
      QuickActionsWidgetEntryView(entry: entry)
    }
    .configurationDisplayName(
      Text("IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_DISPLAY_NAME")
    )
    .description(Text("IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_DESCRIPTION"))
    .supportedFamilies([.systemMedium])
  }
}

struct QuickActionsWidgetEntryView: View {
  var entry: Provider.Entry
  @Environment(\.redactionReasons) var redactionReasons
  private let searchAreaHeight: CGFloat = 92
  private let separatorHeight: CGFloat = 32
  private let incognitoA11yLabel: LocalizedStringKey =
    "IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_INCOGNITO_A11Y_LABEL"
  private let voiceSearchA11yLabel: LocalizedStringKey =
    "IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_VOICE_SEARCH_A11Y_LABEL"
  private let qrA11yLabel: LocalizedStringKey =
    "IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_QR_SCAN_A11Y_LABEL"
  var body: some View {
    VStack(spacing: 0) {
      ZStack {
        Color("widget_background_color")
          .unredacted()
        VStack {
          Spacer()
          Link(destination: WidgetConstants.QuickActionsWidget.searchUrl) {
            ZStack {
              RoundedRectangle(cornerRadius: 26)
                .frame(height: 52)
                .foregroundColor(Color("widget_search_bar_color"))
              HStack(spacing: 12) {
                Image("widget_chrome_logo")
                  .clipShape(Circle())
                  // Without .clipShape(Circle()), in the redacted/placeholder
                  // state the Widget shows an rectangle placeholder instead of
                  // a circular one.
                  .padding(.leading, 8)
                  .unredacted()
                Text("IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_TITLE")
                  .font(.subheadline)
                  .foregroundColor(Color("widget_text_color"))
                Spacer()
              }
            }
            .frame(minWidth: 0, maxWidth: .infinity)
            .padding([.leading, .trailing], 11)
          }
          .accessibility(
            label:
              Text(
                "IDS_IOS_WIDGET_KIT_EXTENSION_QUICK_ACTIONS_SEARCH_A11Y_LABEL"
              )
          )
          Spacer()
        }
        .frame(height: searchAreaHeight)
      }
      ZStack {
        Rectangle()
          .foregroundColor(Color("widget_actions_row_background_color"))
          .frame(minWidth: 0, maxWidth: .infinity)
        HStack {
          // Show interactive buttons if the widget is fully loaded, and show
          // the custom placeholder otherwise.
          if redactionReasons.isEmpty {
            Link(destination: WidgetConstants.QuickActionsWidget.incognitoUrl) {
              Image("widget_incognito_icon")
                .frame(minWidth: 0, maxWidth: .infinity)
            }
            .accessibility(label: Text(incognitoA11yLabel))
            Separator(height: separatorHeight)
            Link(
              destination: WidgetConstants.QuickActionsWidget.voiceSearchUrl
            ) {
              Image("widget_voice_search_icon")
                .frame(minWidth: 0, maxWidth: .infinity)
            }
            .accessibility(label: Text(voiceSearchA11yLabel))
            Separator(height: separatorHeight)
            Link(destination: WidgetConstants.QuickActionsWidget.qrCodeUrl) {
              Image("widget_qr_icon")
                .frame(minWidth: 0, maxWidth: .infinity)
            }
            .accessibility(label: Text(qrA11yLabel))
          } else {
            ButtonPlaceholder()
            Separator(height: separatorHeight)
            ButtonPlaceholder()
            Separator(height: separatorHeight)
            ButtonPlaceholder()
          }
        }
        .frame(minWidth: 0, maxWidth: .infinity)
        .padding([.leading, .trailing], 11)
      }
    }
  }
}

struct Separator: View {
  let height: CGFloat
  var body: some View {
    RoundedRectangle(cornerRadius: 1)
      .foregroundColor(Color("widget_separator_color"))
      .frame(width: 2, height: height)
  }
}

struct ButtonPlaceholder: View {
  private let widthOfPlaceholder: CGFloat = 28
  var body: some View {
    Group {
      RoundedRectangle(cornerRadius: 4, style: .continuous)
        .frame(width: widthOfPlaceholder, height: widthOfPlaceholder)
        .foregroundColor(Color("widget_text_color"))
        .opacity(0.3)
    }.frame(minWidth: 0, maxWidth: .infinity)
  }
}
