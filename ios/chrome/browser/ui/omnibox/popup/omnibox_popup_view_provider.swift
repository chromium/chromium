// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import UIKit

/// A provider to provide the SwiftUI PopupView to Objective-C. This is
/// necessary because Objective-C can't see SwiftUI types.
@objcMembers public class OmniboxPopupViewProvider: NSObject {

  /// Returns a hosting controller embedding the popup view using the given
  /// model and settings.
  ///
  /// - Parameters:
  ///   - model: The popup model to be used by the popup view.
  ///   - popupUIVariation: The UI variant to use for the popup view.
  ///   - popupShouldSelfSize: Whether the popup should resize itself to fit its content.
  ///   - appearanceContainerType: The container type that will contain the popup view, so
  ///     its appearance can be properly styled.
  /// - Returns: The hosting controller which embeds the popup view.
  public static func makeViewController(
    withModel model: PopupModel, popupUIVariation: PopupUIVariation, popupShouldSelfSize: Bool,
    appearanceContainerType: UIAppearanceContainer.Type?
  )
    -> UIViewController & ContentProviding
  {
    let rootView = PopupView(
      model: model, shouldSelfSize: popupShouldSelfSize,
      appearanceContainerType: appearanceContainerType
    ).environment(\.popupUIVariation, popupUIVariation)
    return OmniboxPopupHostingController(rootView: rootView, model: model)
  }
}

class OmniboxPopupHostingController<Content>: UIHostingController<Content>, ContentProviding
where Content: View {
  let model: PopupModel

  init(rootView: Content, model: PopupModel) {
    self.model = model
    super.init(rootView: rootView)
  }

  required init(coder aDecoder: NSCoder) {
    fatalError("Not using storyboards")
  }

  override func viewDidLoad() {
    view.backgroundColor = .clear
    view.isOpaque = false
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()

    guard let guide = NamedGuide(name: kOmniboxGuide, view: view) else {
      return
    }

    // Calculate the leading and trailing space here in UIKit world so SwiftUI
    // gets accurate spacing.
    let frameInView = guide.constrainedView.convert(guide.constrainedView.bounds, to: view)
    model.omniboxLeadingSpace = frameInView.minX
    model.omniboxTrailingSpace = view.bounds.width - frameInView.width - frameInView.minX
  }

  var hasContent: Bool {
    return model.sections.map(\.matches.count).reduce(0, +) > 0
  }
}
