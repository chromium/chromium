// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import CxxImports
import Foundation
import UIKit

// Swift implementation of HistoryCoordinatory.
@MainActor
class SwiftHistoryCoordinatorImpl: HistoryCoordinator, @unchecked Sendable {

  // ViewController being managed by this Coordinator.
  private var historyNavigationController: TableViewNavigationController?
  private var historyTableViewController: HistoryTableViewController?

  override func start() {
    // Initialize and configure HistoryTableViewController.
    let viewController = HistoryTableViewController()!
    viewController.searchTerms = searchTerms
    viewController.delegate = self
    historyTableViewController = viewController

    // Configure and present HistoryNavigationController.
    let navigationController = TableViewNavigationController(table: historyTableViewController)!
    navigationController.isToolbarHidden = false
    navigationController.modalPresentationStyle = .formSheet
    navigationController.presentationController?.delegate = historyTableViewController
    historyNavigationController = navigationController

    super.start()

    baseViewController?.present(navigationController, animated: true, completion: nil)
  }

  // MARK: - BaseHistoryCoordinator

  // This method should always execute the `completionHandler`.
  override func dismiss(completion completionHandler: (() -> Void)?) {
    defer {
      super.dismiss(completion: completionHandler)
    }
    guard let navigationController = historyNavigationController else {
      completionHandler?()
      return
    }
    navigationController.dismiss(animated: true, completion: completionHandler)
    historyNavigationController = nil
    historyTableViewController?.historyService = nil
  }

  // MARK: - Setters & Getters

  override var viewController: BaseHistoryViewController? {
    get {
      return historyTableViewController
    }
    set {
      historyTableViewController = newValue as? HistoryTableViewController
    }
  }

  override var scenario: MenuScenarioHistogram {
    return kMenuScenarioHistogramHistoryEntry
  }
}
