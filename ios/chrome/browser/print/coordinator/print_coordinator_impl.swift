// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import ChromiumCxxStdlib
import CxxImports
import UIKit

// Swift implementation of PrintCoordinator.
@MainActor
@objc
class SwiftPrintCoordinatorImpl: PrintCoordinator,
  UIPrintInteractionControllerDelegate, @unchecked Sendable
{
  // The view controller the system print dialog should be presented from.
  // This can be passed in the print* method or `baseViewController` will
  // be used.
  private weak var defaultBaseViewController: UIViewController?

  // MARK: - Public

  @objc override func dismiss(animated: Bool) {
    UIPrintInteractionController.shared.dismiss(animated: animated)
  }

  // MARK: - ChromeCoordinator

  @objc override func stop() {
    MainActor.assumeIsolated {
      defaultBaseViewController = nil
    }
  }

  // MARK: - PrintHandler

  @objc override func printView(
    _ view: UIView, withTitle title: String
  ) {
    self.printView(view, withTitle: title, baseViewController: self.baseViewController)
  }

  @objc override func printView(
    _ view: UIView, withTitle title: String, baseViewController: UIViewController
  ) {
    let renderer = UIPrintPageRenderer()
    renderer.addPrintFormatter(view.viewPrintFormatter(), startingAtPageAt: 0)

    print(renderer: renderer, item: nil, title: title, baseViewController: baseViewController)
  }

  @objc override func printImage(
    _ image: UIImage, title: String, baseViewController: UIViewController
  ) {
    print(renderer: nil, item: image, title: title, baseViewController: baseViewController)
  }

  // MARK: - UIPrintInteractionControllerDelegate

  func printInteractionControllerParentViewController(
    _ printInteractionController: UIPrintInteractionController
  ) -> UIViewController? {
    return defaultBaseViewController
  }

  // Utility method to print either a renderer or a printable item (as documented
  // in UIPrintInteractionController printingItem).
  // Exactly one of `renderer` and `item` must be not nil.
  private func print(
    renderer: UIPrintPageRenderer?, item: Any?, title: String,
    baseViewController: UIViewController
  ) {
    // Only one item must be passed.
    assert((renderer != nil) != (item != nil))
    self.defaultBaseViewController = baseViewController
    base.swift.RecordUserMetricsAction("MobilePrintMenuAirPrint")
    let printInteractionController = UIPrintInteractionController.shared
    printInteractionController.delegate = self

    let printInfo = UIPrintInfo.printInfo()
    printInfo.outputType = .general
    printInfo.jobName = title
    printInteractionController.printInfo = printInfo

    printInteractionController.printPageRenderer = renderer
    printInteractionController.printingItem = item

    printInteractionController.present(
      animated: true,
      completionHandler: { (controller, completed, error) in
        if let error = error {
          NSLog("Air printing error: %@", error.localizedDescription)
        }
      })
  }
}
