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

    let noAccount = AccountDetail(id: "Default", email: noAccountName)

    guard let accounts = try? await suggestedEntities()
    else { return noAccount }

    // If available, return the primary account as default result.
    guard let sharedDefaults: UserDefaults = AppGroupHelper.groupUserDefaults()
    else { return noAccount }
    guard let primaryAccount = sharedDefaults.object(forKey: "ios.primary_account") as? String
    else { return noAccount }
    for account in accounts {
      if account.id == primaryAccount {
        return AccountDetail(id: account.id, email: account.email)
      }
    }
    return noAccount
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

    let noAccountName = String(
      localized: "IDS_IOS_WIDGET_KIT_EXTENSION_NO_ACCOUNT_LABEL")
    accountsDetail.append(AccountDetail(id: "Default", email: noAccountName))

    for (key, value) in accounts {
      if let email = value["email"] as? String {
        accountsDetail.append(AccountDetail(id: key, email: email))
      }
    }
    return accountsDetail
  }
}

@available(iOS 17, *)
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
    guard let gaia = account?.id
    else { return nil }

    return gaia
  }

  // Returns a boolean used to check if the account was deleted from device.
  func deleted() -> Bool {
    return account?.id == nil
  }
}
