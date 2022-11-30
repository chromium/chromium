// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

/// Fake implementation of `OmniboxIcon` for prototyping/previews or tests.
class FakeOmniboxIcon: NSObject, OmniboxIcon {
  var iconType: OmniboxIconType

  var imageURL: CrURL?

  var iconImage: UIImage?

  var iconImageTintColor: UIColor?

  var backgroundImage: UIImage?

  var backgroundImageTintColor: UIColor?

  var overlayImage: UIImage?

  var overlayImageTintColor: UIColor?

  init(
    iconType: OmniboxIconType,
    imageURL: CrURL? = nil,
    iconImage: UIImage? = nil,
    iconImageTintColor: UIColor? = nil,
    backgroundImage: UIImage? = nil,
    backgroundImageTintColor: UIColor? = nil,
    overlayImage: UIImage? = nil,
    overlayImageTintColor: UIColor? = nil
  ) {
    self.iconType = iconType
    self.imageURL = imageURL
    self.iconImage = iconImage?.withRenderingMode(.alwaysTemplate)
    self.iconImageTintColor = iconImageTintColor
    self.backgroundImage = backgroundImage?.withRenderingMode(.alwaysTemplate)
    self.backgroundImageTintColor = backgroundImageTintColor
    self.overlayImage = overlayImage?.withRenderingMode(.alwaysTemplate)
    self.overlayImageTintColor = overlayImageTintColor
  }
}

extension FakeOmniboxIcon {
  static let suggestionAnswerIcon = FakeOmniboxIcon(
    iconType: .suggestionIcon,
    iconImage: UIImage(named: "answer_dictionary"),
    iconImageTintColor: UIColor(named: "omnibox_suggestion_answer_icon_color"),
    backgroundImage: UIImage(named: "background_solid"),
    backgroundImageTintColor: UIColor(named: "blue_color"))
  static let suggestionIcon = FakeOmniboxIcon(
    iconType: .suggestionIcon,
    iconImage: UIImage(named: "omnibox_completion_history"),
    iconImageTintColor: UIColor(named: "omnibox_suggestion_icon_color"))
  static let favicon = FakeOmniboxIcon(
    iconType: .favicon,
    iconImage: UIImage(named: "favicon_fallback"),
    iconImageTintColor: UIColor(named: "omnibox_suggestion_icon_color"))
}
