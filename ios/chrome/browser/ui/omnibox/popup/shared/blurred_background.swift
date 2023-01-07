// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A view modifier that adds a blurred background to a view. This correctly
/// handles the differences between iOS 14 and 15, as SwiftUI Material wasn't
/// added until iOS 15.
struct BlurredBackground: ViewModifier {
  /// A SwiftUI wrapper for a UIVisualEffectView with a blur effect. Necessary
  /// because the SwiftUI Materials weren't added until iOS 15.
  struct BlurVisualEffectView: UIViewRepresentable {
    func makeUIView(context: UIViewRepresentableContext<Self>) -> UIVisualEffectView {
      UIVisualEffectView()
    }

    func updateUIView(_ view: UIVisualEffectView, context: Self.Context) {
      view.effect = UIBlurEffect(style: .systemUltraThinMaterial)
    }
  }

  @ViewBuilder
  func body(content: Content) -> some View {
    if #available(iOS 15, *) {
      content
        .background(.ultraThinMaterial)
    } else {
      ZStack {
        BlurVisualEffectView()
        content
      }
    }
  }
}

extension View {
  func blurredBackground() -> some View {
    modifier(BlurredBackground())
  }
}
