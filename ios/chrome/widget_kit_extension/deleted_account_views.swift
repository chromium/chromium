// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI

enum DeletedAccountUIConstants {
  static let cornerRadius: CGFloat = 22
  static let padding: CGFloat = 3
  static let whiteBorder: CGFloat = 11
}

// Store in NSUserDefaults that the deleted account view appeared.
func CollectMetricsInfo() {
  guard let sharedDefaults = AppGroupHelper.groupUserDefaults() else { return }
  sharedDefaults.set(true, forKey: "DeletedAccountUiDisplayed")
}

@MainActor
func SmallWidgetDeletedAccountView() -> some View {
  VStack {
    ZStack {
      RoundedRectangle(cornerRadius: DeletedAccountUIConstants.cornerRadius)
        .foregroundColor(Color("widget_search_bar_color"))
        .overlay(
          Text("IDS_IOS_WIDGET_KIT_EXTENSION_DELETED_ACCOUNT")
            .font(.subheadline)
            .foregroundColor(Color("widget_text_color"))
            .multilineTextAlignment(.center)
            .padding(DeletedAccountUIConstants.padding)
        )
    }
    .frame(minWidth: 0, maxWidth: .infinity)
    .padding(DeletedAccountUIConstants.whiteBorder)
  }
  .crContainerBackground(Color("widget_background_color").unredacted())
  .onAppear {
    CollectMetricsInfo()
  }
}

@MainActor
func MediumWidgetDeletedAccountView() -> some View {
  VStack {
    ZStack {
      RoundedRectangle(cornerRadius: DeletedAccountUIConstants.cornerRadius)
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
              .padding(.top, DeletedAccountUIConstants.padding)
          }
        )
    }
    .frame(minWidth: 0, maxWidth: .infinity)
    .padding(DeletedAccountUIConstants.whiteBorder)
  }
  .crContainerBackground(Color("widget_background_color").unredacted())
  .onAppear {
    CollectMetricsInfo()
  }
}
