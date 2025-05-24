// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Combine

// Model class for holding and tracking state for destination customization
// in the overflow menu.
@objcMembers public class DestinationCustomizationModel: NSObject, ObservableObject {
  @Published public var shownDestinations: [OverflowMenuDestination]
  @Published public var hiddenDestinations: [OverflowMenuDestination]

  @Published public var destinationUsageEnabled: Bool

  private let initialData:
    (shown: [OverflowMenuDestination], hidden: [OverflowMenuDestination], usageEnabled: Bool)

  public var hasChanged: Bool {
    return destinationUsageEnabled != initialData.usageEnabled
      || shownDestinations != initialData.shown || hiddenDestinations != initialData.hidden
  }

  /// Holds sinks for all the destination observation.
  var cancellables: Set<AnyCancellable> = []

  public init(destinations: [OverflowMenuDestination], destinationUsageEnabled: Bool) {
    let shownDestinations = destinations.filter(\.shown)
    let hiddenDestinations = destinations.filter { !$0.shown }

    self.shownDestinations = shownDestinations
    self.hiddenDestinations = hiddenDestinations
    self.destinationUsageEnabled = destinationUsageEnabled

    initialData = (
      shown: shownDestinations, hidden: hiddenDestinations, usageEnabled: destinationUsageEnabled
    )

    super.init()

    destinations.forEach { destination in
      // dropFirst, so the sink is only called for subsequent changes, not the
      // initial state.
      destination.$shown.dropFirst().sink { [weak self] newShown in
        self?.toggle(destination: destination, newShown: newShown)
      }.store(in: &cancellables)
    }
  }

  /// Moves `action` to the correct group based on its new `shown`
  /// state.
  func toggle(destination: OverflowMenuDestination, newShown: Bool) {
    destinationUsageEnabled = false
    // If action is now shown, remove it from hiddenActions if necessary
    // and add to shownActions. Otherwise, do the reverse.
    if newShown {
      guard let index = hiddenDestinations.firstIndex(of: destination) else {
        return
      }
      hiddenDestinations.remove(at: index)
      shownDestinations.insert(destination, at: 0)
    } else {
      guard let index = shownDestinations.firstIndex(of: destination) else {
        return
      }
      shownDestinations.remove(at: index)
      hiddenDestinations.append(destination)
    }
  }
}
