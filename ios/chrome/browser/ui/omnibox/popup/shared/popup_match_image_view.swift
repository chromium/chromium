// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// The View displaying a PopupImage, composited correctly.
struct PopupMatchImageView: View {
  enum Dimension {
    /// The width and height of all images.
    static let imageDimension: CGFloat = 30
  }

  /// The image model object for this view.
  @ObservedObject var image: PopupImage

  var body: some View {
    ZStack {
      image.backgroundImage?.foregroundColor(image.backgroundImageTintColor)
      switch image.icon.iconType {
      case .favicon:
        (image.iconImageFromURL ?? image.iconImage).foregroundColor(image.iconImageTintColor)
      case .suggestionIcon:
        image.iconImage?.foregroundColor(image.iconImageTintColor)
      case .image:
        image.iconImageFromURL?.resizable().aspectRatio(contentMode: .fit).foregroundColor(
          image.iconImageTintColor)
      @unknown default:
        image.iconImage?.foregroundColor(image.iconImageTintColor)
      }
    }
    .frame(width: Dimension.imageDimension, height: Dimension.imageDimension)
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
        PopupMatchImageView(image: PopupImage(icon: icon))
      }
    }
    .previewLayout(.sizeThatFits)
  }
}
