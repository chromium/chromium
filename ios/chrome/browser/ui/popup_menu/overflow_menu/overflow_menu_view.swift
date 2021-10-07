// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

struct OverflowMenuView: View {
  @EnvironmentObject var model: OverflowMenuModel
  var body: some View {
    Group {
      OverflowMenuActionList(actions: model.actions)
    }.background(Color(.systemGroupedBackground).edgesIgnoringSafeArea(.top))
  }
}
