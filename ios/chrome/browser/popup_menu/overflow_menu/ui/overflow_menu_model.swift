// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Holds all the data necessary to create the views for the overflow menu.
@objcMembers public class OverflowMenuModel: NSObject, ObservableObject {
  /// Holds all the necessary data for the customization flow in the overflow
  /// menu.
  struct CustomizationModel: Equatable {
    let actions: ActionCustomizationModel
    let destinations: DestinationCustomizationModel
  }

  /// The destinations for the overflow menu.
  @Published public var destinations: [OverflowMenuDestination]

  /// The action groups for the overflow menu.
  @Published public var actionGroups: [OverflowMenuActionGroup]

  /// If present, indicates that the menu should show a customization flow using
  /// the provided data.
  @Published var customization: CustomizationModel?

  /// Whether or not customization is currently in progress.
  public var isCustomizationActive: Bool {
    return customization != nil
  }

  public init(
    destinations: [OverflowMenuDestination],
    actionGroups: [OverflowMenuActionGroup]
  ) {
    self.destinations = destinations
    self.actionGroups = actionGroups
  }

  public func startCustomization(
    actions: ActionCustomizationModel,
    destinations: DestinationCustomizationModel
  ) {
    customization = CustomizationModel(actions: actions, destinations: destinations)
  }

  public func endCustomization() {
    customization = nil
  }

  public func setDestinationsWithAnimation(_ destinations: [OverflowMenuDestination]) {
    withAnimation {
      self.destinations = destinations
    }
  }
}
