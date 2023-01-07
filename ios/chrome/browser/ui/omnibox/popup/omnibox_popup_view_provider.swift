// Copyright 2022 The Chromium Authors
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
  ///   - uiConfiguration: The model object holding ui configuration data for
  ///     the popup view.
  ///   - popupUIVariation: The UI variant to use for the popup view.
  ///   - popupShouldSelfSize: Whether the popup should resize itself to fit its content.
  ///   - appearanceContainerType: The container type that will contain the popup view, so
  ///     its appearance can be properly styled.
  /// - Returns: The hosting controller which embeds the popup view.
  public static func makeViewController(
    withModel model: PopupModel, uiConfiguration: PopupUIConfiguration,
    popupUIVariation: PopupUIVariation, popupPasteButtonVariation: PopupPasteButtonVariation,
    popupShouldSelfSize: Bool, appearanceContainerType: UIAppearanceContainer.Type?
  )
    -> UIViewController & ContentProviding
  {
    let rootView = PopupView(
      model: model, uiConfiguration: uiConfiguration, shouldSelfSize: popupShouldSelfSize,
      appearanceContainerType: appearanceContainerType
    )
    .environment(\.popupUIVariation, popupUIVariation)
    .environment(\.popupPasteButtonVariation, popupPasteButtonVariation)
    return OmniboxPopupHostingController(
      rootView: rootView, model: model, uiConfiguration: uiConfiguration)
  }
}

class OmniboxPopupHostingController<Content>: UIHostingController<Content>, ContentProviding
where Content: View {
  let model: PopupModel
  let uiConfiguration: PopupUIConfiguration

  init(rootView: Content, model: PopupModel, uiConfiguration: PopupUIConfiguration) {
    self.model = model
    self.uiConfiguration = uiConfiguration
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

    let safeAreaGuide = view.safeAreaLayoutGuide
    guard let omniboxGuide = NamedGuide(name: kOmniboxGuide, view: view),
      let omniboxLeadingImageGuide = NamedGuide(name: kOmniboxLeadingImageGuide, view: view),
      let omniboxTextFieldGuide = NamedGuide(name: kOmniboxTextFieldGuide, view: view)
    else {
      return
    }

    // Calculate the leading and trailing space here in UIKit world so SwiftUI
    // gets accurate spacing.
    let omniboxFrameInView = omniboxGuide.constrainedView.convert(
      omniboxGuide.constrainedView.bounds, to: view)

    let isLTR =
      UIApplication.shared.userInterfaceLayoutDirection
      == UIUserInterfaceLayoutDirection.leftToRight

    uiConfiguration.omniboxLeadingSpace =
      isLTR ? omniboxFrameInView.minX : view.frame.size.width - omniboxFrameInView.maxX
    uiConfiguration.omniboxTrailingSpace =
      isLTR ? view.bounds.width - omniboxFrameInView.maxX : omniboxFrameInView.minX

    let safeAreaFrame = safeAreaGuide.layoutFrame
    uiConfiguration.safeAreaTrailingSpace = view.bounds.width - safeAreaFrame.maxX

    let omniboxLeadingImageFrameInView = omniboxLeadingImageGuide.constrainedView.convert(
      omniboxLeadingImageGuide.constrainedView.bounds, to: view)
    uiConfiguration.omniboxLeadingImageLeadingSpace =
      isLTR
      ? omniboxLeadingImageFrameInView.minX - omniboxFrameInView.minX
      : omniboxFrameInView.maxX - omniboxLeadingImageFrameInView.maxX

    let omniboxTextFieldFrameInView = omniboxTextFieldGuide.constrainedView.convert(
      omniboxTextFieldGuide.constrainedView.bounds, to: view)
    uiConfiguration.omniboxTextFieldLeadingSpace =
      isLTR
      ? omniboxTextFieldFrameInView.minX - omniboxFrameInView.minX
      : omniboxFrameInView.maxX - omniboxTextFieldFrameInView.maxX
  }

  var hasContent: Bool {
    return model.sections.map(\.matches.count).reduce(0, +) > 0
  }
}
