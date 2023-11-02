// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

class ViewController: UIViewController {

  @IBOutlet weak var omniboxView: UIView!
  @IBOutlet weak var omniboxTextField: UITextField!

  @IBOutlet weak var popupContainerView: UIView!

  var popupCoordinator: PopupCoordinator?

  override func viewDidLoad() {
    super.viewDidLoad()
    // Do any additional setup after loading the view.

    omniboxView.layer.cornerRadius = 18
  }

  func openPopup() {
    let coordinator = PopupCoordinator(baseViewController: self, containerView: popupContainerView)
    popupCoordinator = coordinator

    coordinator.start()
  }

  func closePopup() {
    popupCoordinator?.stop()
    popupCoordinator = nil
  }

}

// MARK: - UIAction

extension ViewController {

  @IBAction func openPopupTapped(_ sender: Any) {
    openPopup()
  }

  @IBAction func cancelTapped(_ sender: Any) {
    closePopup()
  }

}
