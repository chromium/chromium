// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// An empty UIKit view wrapped in a SwiftUI view,
/// to identity the empty space below the omnibox popup during hit testing.
final class PopupEmptySpaceView: UIView {
  struct View: UIViewRepresentable {
    func makeUIView(context: Context) -> PopupEmptySpaceView {
      PopupEmptySpaceView()
    }

    func updateUIView(_ uiView: PopupEmptySpaceView, context: Context) {
    }
  }
}
