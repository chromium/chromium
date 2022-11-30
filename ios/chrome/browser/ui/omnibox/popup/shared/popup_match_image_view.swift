// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// The View displaying a PopupImage, composited correctly.
struct PopupMatchImageView: View {
  enum Dimension {
    /// The width and height of all images.
    static let image: CGFloat = 30
  }

  /// The image model object for this view.
  @ObservedObject var image: PopupImage

  // Overrides the foreground color of the image. Used for keyboard selection state.
  let highlightColor: Color?

  var body: some View {

    ZStack {
      image.backgroundImage?.foregroundColor(image.backgroundImageTintColor)
      switch image.icon.iconType {
      case .favicon:
        (image.iconImageFromURL ?? image.iconImage).foregroundColor(
          highlightColor ?? image.iconImageTintColor)
      case .suggestionIcon:
        image.iconImage?.foregroundColor(highlightColor ?? image.iconImageTintColor)
      case .image:
        image.iconImageFromURL?.resizable().aspectRatio(contentMode: .fit).foregroundColor(
          highlightColor ?? image.iconImageTintColor)
      @unknown default:
        image.iconImage?.foregroundColor(image.iconImageTintColor)
      }
    }
    .frame(width: Dimension.image, height: Dimension.image)
  }
}

struct PopupMatchImageView_Previews: PreviewProvider {
  static var previews: some View {
    Group {
      ForEach(
        [
          FakeOmniboxIcon.suggestionAnswerIcon, FakeOmniboxIcon.suggestionIcon,
          FakeOmniboxIcon.favicon,
        ], id: \.self
      ) { icon in
        PopupMatchImageView(image: PopupImage(icon: icon), highlightColor: nil)
      }
    }
    .previewLayout(.sizeThatFits)
  }
}
