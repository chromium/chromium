// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

// Spacing before and after the top image in the confirmation alert prompt.
private let kSpacingBeforeImage: CGFloat = 24
private let kSpacingAfterImage: CGFloat = 12

// The half-sheet variation of the "Bring Android Tabs" prompt that uses the
// ConfirmationAlertViewController as a template.
// TODO(crbug.com/427169284): Replace @preconcurrency with @MainActor when all test bots
// support this new syntax. @preconcurrency tells the Swift 6 compiler to do concurrency
// chencking during the run-time instead of the compile time.
@objcMembers
class BringAndroidTabsPromptConfirmationAlertViewController:
  ConfirmationAlertViewController,  // super class
  @preconcurrency ConfirmationAlertActionHandler,  // protocol
  UIAdaptivePresentationControllerDelegate
{
  // Delegate to handle user actions.
  weak var delegate: BringAndroidTabsPromptViewControllerDelegate?
  // Command handler to continue Bring Android Tabs user journey base on command
  // invocation.
  weak var commandHandler: BringAndroidTabsCommands?
  // Number of active tabs from Android.
  private let tabsCount: Int

  init(tabsCount: Int) {
    self.tabsCount = tabsCount
    let configuration = ButtonStackConfiguration()
    configuration.primaryActionString = L10nUtils.pluralString(
      messageId: IDS_IOS_BRING_ANDROID_TABS_PROMPT_OPEN_TABS_BUTTON,
      number: tabsCount)
    configuration.secondaryActionString = L10nUtils.pluralString(
      messageId: IDS_IOS_BRING_ANDROID_TABS_CANCEL_BUTTON,
      number: tabsCount)
    configuration.tertiaryActionString = L10nUtils.pluralString(
      messageId:
        IDS_IOS_BRING_ANDROID_TABS_PROMPT_REVIEW_TABS_BUTTON_CONFIRMATION_ALERT,
      number: tabsCount)
    super.init(configuration: configuration)
  }

  override func viewDidLoad() {
    // Configure confirmation alert properties. These properties need to be set
    // before calling the confirmation alert's `viewDidLoad`.
    self.image = UIImage(named: "bring_android_tabs_icon")
    self.imageHasFixedSize = true
    self.customSpacingBeforeImageIfNoNavigationBar = kSpacingBeforeImage
    self.customSpacingAfterImage = kSpacingAfterImage
    self.titleString = L10nUtils.pluralString(
      messageId: IDS_IOS_BRING_ANDROID_TABS_PROMPT_TITLE,
      number: self.tabsCount)
    self.titleTextStyle = .title2
    self.subtitleString = L10nUtils.pluralString(
      messageId: IDS_IOS_BRING_ANDROID_TABS_PROMPT_SUBTITLE,
      number: self.tabsCount)
    super.viewDidLoad()
    // Configure properties specific to the "Bring Android Tabs" prompt.
    self.actionHandler = self
    self.presentationController?.delegate = self
    self.overrideUserInterfaceStyle = .dark
    self.view.accessibilityIdentifier =
      kBringAndroidTabsPromptConfirmationAlertAXId
  }

  override func viewDidAppear(_ animated: Bool) {
    super.viewDidAppear(animated)
    self.delegate?.bringAndroidTabsPromptViewControllerDidShow()
  }

  // MARK: - UIAdaptivePresentationControllerDelegate

  func presentationControllerDidDismiss(_ controller: UIPresentationController) {
    self.delegate?.bringAndroidTabsPromptViewControllerDidDismiss(swiped: true)
    self.commandHandler?.dismissBringAndroidTabsPrompt()
  }

  // MARK: - ConfirmationAlertActionHandler

  func confirmationAlertPrimaryAction() {
    self.delegate?.bringAndroidTabsPromptViewControllerDidTapOpenAllButton()
    self.commandHandler?.dismissBringAndroidTabsPrompt()
  }

  func confirmationAlertSecondaryAction() {
    self.delegate?.bringAndroidTabsPromptViewControllerDidDismiss(swiped: false)
    self.commandHandler?.dismissBringAndroidTabsPrompt()
  }

  func confirmationAlertTertiaryAction() {
    self.delegate?.bringAndroidTabsPromptViewControllerDidTapReviewButton()
    self.commandHandler?.reviewAllBringAndroidTabs()
  }
}
