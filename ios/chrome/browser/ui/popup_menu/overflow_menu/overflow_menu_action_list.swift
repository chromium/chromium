// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A view that displays a list of actions in the overflow menu.
struct OverflowMenuActionList: View {
  /// The list of action groups for this view.
  var actionGroups: [OverflowMenuActionGroup]

  var body: some View {
    List {
      ForEach(actionGroups) { actionGroup in
        OverflowMenuActionSection(actionGroup: actionGroup)
      }
    }
    .listStyle(InsetGroupedListStyle())
    // Allow sections to have very small headers controlling section spacing.
    .environment(\.defaultMinListHeaderHeight, 0)
  }
}
