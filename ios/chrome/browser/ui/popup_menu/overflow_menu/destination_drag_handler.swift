// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// Class to handle destination drag interactions in the overflow menu
/// customization flow.
class DestinationDragHandler: ObservableObject {
  /// Collection of data for the current drag interaction.
  struct CurrentDragData {
    var item: OverflowMenuDestination
    var initialIndex: Int
    var initialDestinationUsageEnabled: Bool
  }

  /// The destination customization model underlying the session.
  @ObservedObject var destinationModel: DestinationCustomizationModel

  /// The data for the current drag session. `nil` if nothing is being dragged.
  @Published private(set) var currentDrag: CurrentDragData? = nil

  /// Whether the drag is currently over the destinations row. This is necessary
  /// because the drag system doesn't currently offer a way to tell in general
  /// when a drag ends, so the state needs to be correct for whenever the drag
  /// happens to end.
  @Published var dragOnDestinations = false

  /// Stores all currently existing item providers weakly.
  var providers: [WeakItemProviderContainer] = []

  init(destinationModel: DestinationCustomizationModel) {
    self.destinationModel = destinationModel
  }

  /// Begin a new drag of the given `destination`.
  func startDrag(from item: OverflowMenuDestination) {
    // End any existing drag in case it wasn't able to be ended before.
    endDrag()
    guard let index = destinationModel.shownDestinations.firstIndex(of: item) else {
      return
    }
    currentDrag = CurrentDragData(
      item: item, initialIndex: index,
      initialDestinationUsageEnabled: destinationModel.destinationUsageEnabled)
  }

  /// Ends the current drag state.
  func endDrag() {
    currentDrag = nil
    dragOnDestinations = false
  }

  /// Returns a new drop delegate for drop actions on the provided
  /// `destination`.
  func newDropDelegate(forDestination destination: OverflowMenuDestination)
    -> DestinationDropDelegate
  {
    return DestinationDropDelegate(handler: self, destination: destination)
  }

  /// Returns a new drop delegate for the entire destination list.
  func newDestinationListDropDelegate() -> DestinationListDropDelegate {
    return DestinationListDropDelegate(handler: self)
  }

  /// Returns a new item provider for the drag interaction.
  func newItemProvider(forDestination destination: OverflowMenuDestination) -> NSItemProvider {
    let itemProvider = DidEndItemProvider(object: destination.name as NSString)
    itemProvider.delegate = self
    // When VoiceOver is enabled, sometimes the system requests 2 providers
    // before deallocating one, so storing all existing providers is required
    // to know when all providers have been destroyed.
    providers.append(WeakItemProviderContainer(provider: itemProvider))
    return itemProvider
  }

  /// Performs a drop into a list. Should be passed to `List`'s `.onInsert`
  /// method.
  func performListDrop(index: Int, providers: [NSItemProvider]) {
    currentDrag?.item.shown = false
  }

  /// Drop delegate for an individual destination. This is used for drop actions
  /// when the user's finger is over the given destination.
  struct DestinationDropDelegate: DropDelegate {
    let handler: DestinationDragHandler
    let destination: OverflowMenuDestination

    func dropUpdated(info: DropInfo) -> DropProposal? {
      return DropProposal(operation: .move)
    }

    /// Ends the current drag.
    func performDrop(info: DropInfo) -> Bool {
      // Any necessary state has already been updated, so just stop the current
      // drag.
      handler.endDrag()
      return true
    }

    /// Updates the state when the user's finger enters the given destination.
    func dropEntered(info: DropInfo) {
      // Moves the dragged destination to the position where the user's finger
      // is and updates the `destinationUsageEnabled` property if the order has
      // changed.
      guard let currentDrag = handler.currentDrag else {
        return
      }
      handler.dragOnDestinations = true
      guard
        let fromIndex = handler.destinationModel.shownDestinations.firstIndex(
          of: currentDrag.item)
      else {
        return
      }
      handler.destinationModel.destinationUsageEnabled =
        currentDrag.initialDestinationUsageEnabled && fromIndex == currentDrag.initialIndex
      guard let toIndex = handler.destinationModel.shownDestinations.firstIndex(of: destination)
      else {
        return
      }
      if fromIndex != toIndex {
        withAnimation {
          self.handler.destinationModel.shownDestinations.move(
            fromOffsets: IndexSet(integer: fromIndex),
            toOffset: toIndex > fromIndex ? toIndex + 1 : toIndex)
        }
      }
    }
  }

  /// Drop delegate for the entire destination list. This allow making changes
  /// for if the user drags the destination out of the list.
  struct DestinationListDropDelegate: DropDelegate {
    let handler: DestinationDragHandler

    func dropUpdated(info: DropInfo) -> DropProposal? {
      return DropProposal(operation: .cancel)
    }

    /// Ends the current drag. Any necessary state has already been updated.
    func performDrop(info: DropInfo) -> Bool {
      handler.endDrag()
      return true
    }

    /// Updates the drag state for when the user's finger leaves the row.
    func dropExited(info: DropInfo) {
      // As the system does not notify the app when the drag directly ends, any
      // preparation for a drag ending in random space must be set up here.
      handler.dragOnDestinations = false
    }

    /// Updates the drag state for when the user's finger enters the row.
    func dropEntered(info: DropInfo) {
      // Set up the correct state for if a drag re-enters the destinations list.
      handler.dragOnDestinations = true
    }
  }
}

extension DestinationDragHandler: DidEndItemProviderDelegate {
  func didEndItemProviderDidEnd(_ provider: DidEndItemProvider) {
    providers = providers.filter { container in
      container.provider != nil && container.provider != provider
    }
    if providers.isEmpty {
      endDrag()
    }
  }
}

protocol DidEndItemProviderDelegate: AnyObject {
  /// Alerts the delegate when the item provider is being deinitialized.
  func didEndItemProviderDidEnd(_ provider: DidEndItemProvider)
}

/// A custom item provider class that alerts its delegate when it is about to
/// be de-initialized.
class DidEndItemProvider: NSItemProvider {
  weak var delegate: DidEndItemProviderDelegate?
  deinit {
    delegate?.didEndItemProviderDidEnd(self)
  }
}

/// Container class that holds a weak `DidEndItemProvider`, allowing for
/// collections to hold this weakly.
class WeakItemProviderContainer {
  weak var provider: DidEndItemProvider?
  init(provider: DidEndItemProvider?) {
    self.provider = provider
  }
}
