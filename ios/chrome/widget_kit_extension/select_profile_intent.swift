// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import AppIntents
import Foundation
import SwiftUI
import WidgetKit

struct ProfileQuery: EntityQuery {
  func entities(for identifiers: [String]) async throws -> [ProfileDetail] {
    ProfileDetail.allProfiles().filter { identifiers.contains($0.id) }
  }

  func suggestedEntities() async throws -> [ProfileDetail] {
    ProfileDetail.allProfiles()
  }

  func defaultResult() async -> ProfileDetail? {
    if let firstAccount = try? await suggestedEntities().first {
      return firstAccount
    } else {
      return ProfileDetail(id: "No account", gaia: "Default")
    }
  }
}

struct ProfileDetail: AppEntity {
  let id: String
  let gaia: String

  static var typeDisplayRepresentation: TypeDisplayRepresentation = "Profile"
  static var defaultQuery = ProfileQuery()

  var displayRepresentation: DisplayRepresentation {
    DisplayRepresentation(title: "\(id)")
  }

  static func allProfiles() -> [ProfileDetail] {
    guard let sharedDefaults: UserDefaults = AppGroupHelper.groupUserDefaults()
    else { return [] }
    guard
      let profiles = sharedDefaults.object(forKey: "ios.registered_accounts_on_device")
        as? [String: [String: Any]]
    else { return [] }

    var profilesDetail: [ProfileDetail] = []

    profilesDetail.append(ProfileDetail(id: "No account", gaia: "Default"))

    for (key, value) in profiles {
      if let email = value["email"] as? String {
        profilesDetail.append(ProfileDetail(id: email, gaia: key))
      }
    }
    return profilesDetail
  }
}

@available(iOS 17, *)
struct SelectProfileIntent: WidgetConfigurationIntent {
  static var title: LocalizedStringResource = "Select Profile"
  static var description = IntentDescription("Selects the profile to display shortcuts for.")

  @Parameter(title: "Profile")
  var profile: ProfileDetail?

  // Returns the avatar linked to the account.
  func avatarForAccount(account: ProfileDetail?) -> Image? {
    guard let gaia = profile?.gaia
    else { return nil }

    let avatarFilePath =
      AppGroupHelper.widgetsAvatarFolder().appendingPathComponent("\(gaia).png")

    guard let uiImage = UIImage(contentsOfFile: avatarFilePath.path) else {
      return nil
    }
    return Image(uiImage: uiImage)
  }

  // Returns the gaiaID linked to the account.
  func gaiaForAccount(account: ProfileDetail?) -> String? {
    guard let gaia = profile?.gaia
    else { return nil }

    return gaia
  }
}
