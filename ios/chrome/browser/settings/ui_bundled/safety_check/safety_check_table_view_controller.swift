// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIKit

// Delegate for presentation events related to SafetyCheckTableViewController.
@objc
protocol SafetyCheckTableViewControllerPresentationDelegate: AnyObject {

  // Called when the view controller is removed from its parent.
  func safetyCheckTableViewControllerDidRemove(_ controller: SafetyCheckTableViewController)

}

// Workaround to start the section identifier enum with the base value that's in
// shared ObjC code. Swift doesn't allow named constants in this context.
// Need to remember to use |settingsRawValue| instead of |rawValue|.
protocol SettingsEnum: RawRepresentable where RawValue == Int {}
extension SettingsEnum {
  var settingsRawValue: Int {
    // TODO(crbug.com/40210643): This fails on offical builders when trying
    // to use the ObjC constant. Hard-code it as a workaround.
    return self.rawValue + 10  // kSectionIdentifierEnumZero
  }
}

@objc
class SafetyCheckTableViewController: SettingsRootTableViewController, SafetyCheckConsumer {
  // The accessibility identifier of the safety check table view.
  @objc static let accessibilityIdentifier = "kSafetyCheckTableViewId"

  @objc weak var presentationDelegate: SafetyCheckTableViewControllerPresentationDelegate?

  // Handler for taps on items on the safety check page.
  @objc weak var serviceDelegate: SafetyCheckServiceDelegate?

  // MARK: Private enums

  enum SettingsIdentifier: Int, SettingsEnum {
    case checkTypes
    case checkStart
    case notificationsOptIn
  }

  // MARK: Private members

  // Current state of array of items that form the safety check.
  private var checkTypesItems: [TableViewItem]? {
    didSet {
      reloadData()
    }
  }

  // Header for the safety check page.
  private var safetyCheckHeaderItem: TableViewLinkHeaderFooterItem? {
    didSet {
      reloadData()
    }
  }

  // Current display state of the check start item.
  private var checkStartItem: TableViewItem? {
    didSet {
      reloadData()
    }
  }

  // Footer with timestamp for the safety check page.
  private var safetyCheckFooterItem: TableViewLinkHeaderFooterItem? {
    didSet {
      reloadData()
    }
  }

  // Notifications opt-in button.
  private var notificationsOptInItem: TableViewItem? {
    didSet {
      reloadData()
    }
  }

  override func viewDidLoad() {
    super.viewDidLoad()
    self.tableView.accessibilityIdentifier = SafetyCheckTableViewController.accessibilityIdentifier
    self.title = L10nUtils.string(messageId: IDS_OPTIONS_ADVANCED_SECTION_TITLE_SAFETY_CHECK)
  }

  // MARK: SafetyCheckConsumer

  func setCheck(_ items: [TableViewItem]) {
    checkTypesItems = items
  }

  func setSafetyCheckHeaderItem(_ item: TableViewLinkHeaderFooterItem!) {
    safetyCheckHeaderItem = item
  }

  func setCheckStart(_ item: TableViewItem!) {
    checkStartItem = item
  }

  func setTimestampFooterItem(_ item: TableViewLinkHeaderFooterItem!) {
    safetyCheckFooterItem = item
  }

  func setNotificationsOptIn(_ item: TableViewItem!) {
    notificationsOptInItem = item
  }

  // MARK: LegacyChromeTableViewController

  override func loadModel() {
    super.loadModel()

    if let items = self.checkTypesItems {
      let checkTypes = SettingsIdentifier.checkTypes.settingsRawValue
      if items.count != 0 {
        self.tableViewModel.addSection(withIdentifier: checkTypes)
        for item in items {
          self.tableViewModel.add(item, toSectionWithIdentifier: checkTypes)
        }
      }

      if let item = self.safetyCheckHeaderItem {
        self.tableViewModel.setHeader(item, forSectionWithIdentifier: checkTypes)
      }
    }

    if let item = self.checkStartItem {
      let checkStart = SettingsIdentifier.checkStart.settingsRawValue
      self.tableViewModel.addSection(withIdentifier: checkStart)
      self.tableViewModel.add(item, toSectionWithIdentifier: checkStart)
      if let footerItem = self.safetyCheckFooterItem {
        self.tableViewModel.setFooter(footerItem, forSectionWithIdentifier: checkStart)
      }
    }

    if let item = self.notificationsOptInItem {
      let notificationsOptIn = SettingsIdentifier.notificationsOptIn.settingsRawValue
      self.tableViewModel.addSection(withIdentifier: notificationsOptIn)
      self.tableViewModel.add(item, toSectionWithIdentifier: notificationsOptIn)
    }
  }

  // MARK: UIViewController

  override func didMove(toParent parent: UIViewController?) {
    super.didMove(toParent: parent)
    if parent == nil {
      self.presentationDelegate?.safetyCheckTableViewControllerDidRemove(self)
    }
  }

  // MARK: UITableViewDelegate

  override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
    super.tableView(tableView, didSelectRowAt: indexPath)
    let item = self.tableViewModel.item(at: indexPath)
    self.serviceDelegate?.didSelect(item)
    tableView.deselectRow(at: indexPath, animated: false)
  }

  override func tableView(
    _ tableView: UITableView,
    shouldHighlightRowAt indexPath: IndexPath
  ) -> Bool {
    let item = self.tableViewModel.item(at: indexPath)
    return self.serviceDelegate?.isItemClickable(item) ?? false
  }

  // MARK: UITabelViewDataSource

  override func tableView(
    _ tableView: UITableView,
    cellForRowAt indexPath: IndexPath
  ) -> UITableViewCell {
    let cell = super.tableView(tableView, cellForRowAt: indexPath)
    let tableItem = self.tableViewModel.item(at: indexPath)
    if let delegate = self.serviceDelegate,
      let item = tableItem,
      delegate.isItemWithErrorInfo(item),
      let settingsCheckCell = cell as? SettingsCheckCell
    {
      settingsCheckCell.infoButton.tag = item.type
      settingsCheckCell.infoButton.addTarget(
        self,
        action: #selector(didTapErrorInfoButton),
        for: .touchUpInside)
    }
    return cell
  }

  // MARK: Private

  // Called when user tapped on the information button of an item. Shows popover with detailed
  // description of an error if needed.
  @objc func didTapErrorInfoButton(sender: UIButton) {
    self.serviceDelegate?.infoButtonWasTapped(sender, usingItemType: sender.tag)
  }
}
