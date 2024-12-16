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
    try? await suggestedEntities().first
  }
}

struct ProfileDetail: AppEntity {
  let id: String

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
        as? [String: [String: String]]
    else { return [] }

    var profilesDetail: [ProfileDetail] = []

    for innerDict in profiles.values {
      if let value = innerDict["email"] {
        profilesDetail.append(ProfileDetail(id: value))
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

  init(profile: ProfileDetail) {
    self.profile = profile
  }

  init() {
  }
}
