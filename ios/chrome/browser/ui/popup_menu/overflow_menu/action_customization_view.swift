// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// View for showing the customization screen for overflow actions
struct ActionCustomizationView: View {
  @ObservedObject var actionCustomizationModel: ActionCustomizationModel

  var body: some View {
    OverflowMenuActionList(
      actionGroups: [
        actionCustomizationModel.shownActions, actionCustomizationModel.hiddenActions,
      ], metricsHandler: nil
    )
    .environment(\.editMode, .constant(.active))
  }
}
