// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import AppIntents
import Foundation
import SwiftUI
import WidgetKit

struct AccountQuery: EntityQuery {
  func entities(for identifiers: [String]) async throws -> [AccountDetail] {
    AccountDetail.allAccounts().filter { identifiers.contains($0.id) }
  }

  func suggestedEntities() async throws -> [AccountDetail] {
    AccountDetail.allAccounts()
  }

  func defaultResult() async -> AccountDetail? {
    let defaultAccountName = String(
      localized: "IDS_IOS_WIDGET_KIT_EXTENSION_DEFAULT_ACCOUNT_LABEL")
    return AccountDetail(id: "Default", email: defaultAccountName)
  }
}

struct AccountDetail: AppEntity {
  // Id contains the gaia id of the account or "Default" for the "No account" case.
  let id: String
  let email: String

  static let typeDisplayRepresentation: TypeDisplayRepresentation = "Account"
  static let defaultQuery = AccountQuery()

  var displayRepresentation: DisplayRepresentation {
    DisplayRepresentation(title: "\(email)")
  }

  static func allAccounts() -> [AccountDetail] {
    guard let sharedDefaults: UserDefaults = AppGroupHelper.groupUserDefaults()
    else { return [] }
    guard
      let accounts = sharedDefaults.object(forKey: "ios.registered_accounts_on_device")
        as? [String: [String: Any]]
    else { return [] }

    var accountsDetail: [AccountDetail] = []

    let defaultAccountName = String(
      localized: "IDS_IOS_WIDGET_KIT_EXTENSION_DEFAULT_ACCOUNT_LABEL")
    accountsDetail.append(AccountDetail(id: "Default", email: defaultAccountName))

    // Only add the Default account to the list of accounts if multi-profile flag is not enabled.
    if !MultiprofileEnabled() { return accountsDetail }

    let noAccountName = String(
      localized: "IDS_IOS_WIDGET_KIT_EXTENSION_NO_ACCOUNT_LABEL")
    accountsDetail.append(AccountDetail(id: "No account", email: noAccountName))

    for (key, value) in accounts {
      if let email = value["email"] as? String {
        accountsDetail.append(AccountDetail(id: key, email: email))
      }
    }
    return accountsDetail
  }

  static func MultiprofileEnabled() -> Bool {
    guard let appGroup = AppGroupHelper.groupUserDefaults() else { return false }

    guard let extensionsPrefs = appGroup.object(forKey: "Extension.FieldTrial") as? NSDictionary
    else { return false }

    guard
      let shortcutsWidgetPrefs = extensionsPrefs.object(forKey: "MultiprofileKey")
        as? NSDictionary
    else { return false }
    guard
      let shortcutsWidgetEnabled = shortcutsWidgetPrefs.object(forKey: "FieldTrialValue")
        as? NSNumber
    else { return false }
    return shortcutsWidgetEnabled == 1
  }
}

struct SelectAccountIntent: WidgetConfigurationIntent {
  static let title: LocalizedStringResource = "Select Account"
  static let description = IntentDescription(
    "Selects the account to display shortcuts for.")

  @Parameter(title: "IDS_IOS_WIDGET_KIT_EXTENSION_SELECT_ACCOUNT_LABEL")
  var account: AccountDetail?

  // Returns the avatar linked to the account.
  func avatar() -> Image? {
    guard let gaia = account?.id
    else { return nil }

    let avatarFilePath =
      AppGroupHelper.widgetsAvatarFolder().appendingPathComponent("\(gaia).png")

    guard let uiImage = UIImage(contentsOfFile: avatarFilePath.path) else {
      return nil
    }
    return Image(uiImage: uiImage)
  }

  // Returns the gaiaID linked to the account.
  func gaia() -> String? {
    return account?.id
  }

  // Returns the email linked to the account.
  func email() -> String? {
    return account?.email
  }

  // Returns a boolean used to check if the account was deleted from device.
  func deleted() -> Bool {
    return account?.id == nil
  }
}
