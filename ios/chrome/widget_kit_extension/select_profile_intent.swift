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
    let noAccountName = String(
      localized: "IDS_IOS_WIDGET_KIT_EXTENSION_NO_ACCOUNT_LABEL")

    let noAccount = AccountDetail(id: noAccountName, gaia: "Default")

    guard let accounts = try? await suggestedEntities()
    else { return noAccount }

    // If available, return the primary account as default result.
    guard let sharedDefaults: UserDefaults = AppGroupHelper.groupUserDefaults()
    else { return noAccount }
    guard let primaryAccount = sharedDefaults.object(forKey: "ios.primary_account") as? String
    else { return noAccount }
    for account in accounts {
      if account.gaia == primaryAccount {
        return AccountDetail(id: account.id, gaia: account.gaia)
      }
    }
    return noAccount
  }
}

struct AccountDetail: AppEntity {
  let id: String
  let gaia: String

  static var typeDisplayRepresentation: TypeDisplayRepresentation = "Account"
  static var defaultQuery = AccountQuery()

  var displayRepresentation: DisplayRepresentation {
    DisplayRepresentation(title: "\(id)")
  }

  static func allAccounts() -> [AccountDetail] {
    guard let sharedDefaults: UserDefaults = AppGroupHelper.groupUserDefaults()
    else { return [] }
    guard
      let accounts = sharedDefaults.object(forKey: "ios.registered_accounts_on_device")
        as? [String: [String: Any]]
    else { return [] }

    var accountsDetail: [AccountDetail] = []

    let noAccountName = String(
      localized: "IDS_IOS_WIDGET_KIT_EXTENSION_NO_ACCOUNT_LABEL")
    accountsDetail.append(AccountDetail(id: noAccountName, gaia: "Default"))

    for (key, value) in accounts {
      if let email = value["email"] as? String {
        accountsDetail.append(AccountDetail(id: email, gaia: key))
      }
    }
    return accountsDetail
  }
}

@available(iOS 17, *)
struct SelectAccountIntent: WidgetConfigurationIntent {
  static var title: LocalizedStringResource = "Select Account"
  static var description = IntentDescription("Selects the account to display shortcuts for.")

  @Parameter(title: "IDS_IOS_WIDGET_KIT_EXTENSION_SELECT_ACCOUNT_LABEL")
  var account: AccountDetail?

  // Returns the avatar linked to the account.
  func avatar() -> Image? {
    guard let gaia = account?.gaia
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
    guard let gaia = account?.gaia
    else { return nil }

    return gaia
  }

  // Returns a boolean used to check if the account was deleted from device.
  func deleted() -> Bool {
    return account?.gaia == nil
  }
}
