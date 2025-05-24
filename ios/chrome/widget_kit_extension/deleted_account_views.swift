// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI

enum DeletedAccountUIConstants {
  static let cornerRadius: CGFloat = 22
  static let smallWidgetHeight: CGFloat = 140
  static let smallWidgetWidth: CGFloat = 140
  static let mediumWidgetHeight: CGFloat = 130
  static let mediumWidgetWidth: CGFloat = 310
  static let padding: CGFloat = 8
}

// Store in NSUserDefaults that the deleted account view appeared.
func CollectMetricsInfo() {
  guard let sharedDefaults = AppGroupHelper.groupUserDefaults() else { return }
  sharedDefaults.set(true, forKey: "DeletedAccountUiDisplayed")
}

func SmallWidgetDeletedAccountView() -> some View {
  VStack {
    ZStack {
      RoundedRectangle(cornerRadius: DeletedAccountUIConstants.cornerRadius)
        .frame(
          width: DeletedAccountUIConstants.smallWidgetHeight,
          height: DeletedAccountUIConstants.smallWidgetHeight
        )
        .foregroundColor(Color("widget_search_bar_color"))
        .overlay(
          Text("IDS_IOS_WIDGET_KIT_EXTENSION_DELETED_ACCOUNT")
            .font(.subheadline)
            .foregroundColor(Color("widget_text_color"))
            .multilineTextAlignment(.center)
            .padding(DeletedAccountUIConstants.padding)
        )
    }
  }
  .crContainerBackground(Color("widget_background_color").unredacted())
  .onAppear {
    CollectMetricsInfo()
  }
}

func MediumWidgetDeletedAccountView() -> some View {
  VStack {
    ZStack {
      RoundedRectangle(cornerRadius: DeletedAccountUIConstants.cornerRadius)
        .frame(
          width: DeletedAccountUIConstants.mediumWidgetWidth,
          height: DeletedAccountUIConstants.mediumWidgetHeight
        )
        .foregroundColor(Color("widget_search_bar_color"))
        .overlay(
          VStack {
            Image("widget_chrome_logo")
              .clipShape(Circle())
              .padding(.top, DeletedAccountUIConstants.padding)
              .unredacted()
            Text("IDS_IOS_WIDGET_KIT_EXTENSION_DELETED_ACCOUNT")
              .font(.subheadline)
              .foregroundColor(Color("widget_text_color"))
              .multilineTextAlignment(.center)
              .padding(DeletedAccountUIConstants.padding)
          }
        )
    }
  }
  .crContainerBackground(Color("widget_background_color").unredacted())
  .onAppear {
    CollectMetricsInfo()
  }
}
