// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

struct OverflowMenuView: View {
  var body: some View {
    Group {
      Rectangle()
        .fill(Color.red)
        .frame(width: 40, height: 40)
    }.background(Color(.systemGroupedBackground).edgesIgnoringSafeArea(.top))
  }
}
