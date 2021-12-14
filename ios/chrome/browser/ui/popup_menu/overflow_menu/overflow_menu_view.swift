// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

struct OverflowMenuView: View {
  enum Dimensions {
    static let destinationListHeight: CGFloat = 123
  }

  @EnvironmentObject var model: OverflowMenuModel
  var body: some View {
    VStack(
      alignment: .leading,
      // Leave no spaces above or below Divider, the two other sections will
      // include proper spacing.
      spacing: 0
    ) {
      OverflowMenuDestinationList(destinations: model.destinations)
        .frame(height: Dimensions.destinationListHeight)
      Divider()
      OverflowMenuActionList(actionGroups: model.actionGroups)
    }.background(Color(.systemGroupedBackground).edgesIgnoringSafeArea(.all))
  }
}
