// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import UIKit

class PopupCoordinator {
  weak var baseViewController: UIViewController?
  weak var containerView: UIView?

  var viewController: UIViewController?

  var mediator: PopupMediator?

  init(baseViewController: UIViewController, containerView: UIView) {
    self.baseViewController = baseViewController
    self.containerView = containerView
  }

  func start() {
    guard let baseViewController = baseViewController,
      let containerView = containerView
    else {
      return
    }

    containerView.isHidden = false

    let mediator = PopupMediator()
    self.mediator = mediator

    let viewController = UIHostingController(
      rootView: PopupView(
        model: mediator.model,
        uiConfiguration: PopupUIConfiguration.previewsConfiguration()
      ).environment(\.popupUIVariation, .one))
    self.viewController = viewController
    viewController.view.translatesAutoresizingMaskIntoConstraints = false

    baseViewController.addChild(viewController)
    containerView.addSubview(viewController.view)

    NSLayoutConstraint.activate([
      containerView.topAnchor.constraint(equalTo: viewController.view.topAnchor),
      containerView.bottomAnchor.constraint(equalTo: viewController.view.bottomAnchor),
      containerView.leadingAnchor.constraint(equalTo: viewController.view.leadingAnchor),
      containerView.trailingAnchor.constraint(equalTo: viewController.view.trailingAnchor),
    ])

    viewController.didMove(toParent: baseViewController)
  }

  func stop() {
    containerView?.isHidden = true
    viewController?.willMove(toParent: nil)
    viewController?.view.removeFromSuperview()
    viewController?.removeFromParent()

    mediator = nil
  }
}
