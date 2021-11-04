// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A SwiftUI view for the overflow menu displaying a subsection of the actions list.
struct OverflowMenuActionSection: View {
  @ObservedObject var actionGroup: OverflowMenuActionGroup

  var body: some View {
    let footer = actionGroup.footer.flatMap { actionFooter in
      OverflowMenuFooterRow(footer: actionFooter)
    }
    Section(footer: footer) {
      ForEach(actionGroup.actions) { action in
        OverflowMenuActionRow(action: action)
      }
    }
  }
}
