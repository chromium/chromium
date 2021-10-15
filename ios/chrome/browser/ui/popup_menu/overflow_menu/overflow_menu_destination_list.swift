// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A view displaying a list of destinations.
struct OverflowMenuDestinationList: View {
  /// The destinations for this view.
  var destinations: [OverflowMenuDestination]

  var body: some View {
    ScrollView(.horizontal, showsIndicators: false) {
      LazyHStack(spacing: 0) {
        ForEach(destinations) { destination in
          OverflowMenuDestinationView(destination: destination, iconSpacing: 16)
        }
      }
    }
  }
}
