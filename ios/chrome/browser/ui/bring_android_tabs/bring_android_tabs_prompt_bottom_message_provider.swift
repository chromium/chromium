// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import UIKit

// Provider object to configure the "Bring Android Tabs" bottom message prompt
// and create a view controller. This is necessary because Objective C can't
// see SwiftUI types.
@objcMembers
class BringAndroidTabsPromptBottomMessageProvider: NSObject {
  // Delegate to handle user actions and update browser models accordingly.
  weak var delegate: BringAndroidTabsPromptViewControllerDelegate?
  // Command handler to continue Bring Android Tabs user journey based on
  // command invocation.
  weak var commandHandler: BringAndroidTabsCommands?
  // Number of active tabs from Android.
  let tabsCount: Int

  init(tabsCount: Int) {
    self.tabsCount = tabsCount
  }

  // View controller that manages this view. Should be added to the view
  // hierarchy when the view is displayed or removed.
  lazy var viewController: UIViewController = {
    var viewController = UIHostingController(
      rootView: BringAndroidTabsPromptBottomMessageView(
        tabsCount: self.tabsCount, provider: self))
    // Workaround of SwiftUI/UIKit interoperality limitation that the topmost
    // view always have an opaque background.
    viewController.view.backgroundColor = UIColor.clear
    return viewController
  }()
}
