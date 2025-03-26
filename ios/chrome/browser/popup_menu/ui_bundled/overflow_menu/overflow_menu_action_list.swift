// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI

/// A view that displays a list of actions in the overflow menu.
struct OverflowMenuActionList: View {
  /// The list of action groups for this view.
  var actionGroups: [OverflowMenuActionGroup]

  /// The metrics handler to alert when the user takes metrics actions.
  weak var metricsHandler: PopupMenuMetricsHandler?

  @ObservedObject var uiConfiguration: OverflowMenuUIConfiguration

  /// The namespace for the animation of this view appearing or disappearing.
  let namespace: Namespace.ID

  var body: some View {
    ScrollViewReader { scrollProxy in
      List {
        let nonEmpty = actionGroups.filter({ !$0.actions.isEmpty })
        ForEach(nonEmpty) { actionGroup in
          let isLast = actionGroup == nonEmpty.last
          OverflowMenuActionSection(
            actionGroup: actionGroup, metricsHandler: metricsHandler,
            footerBackground: {
              if isLast {
                Color.clear.onAppear {
                  metricsHandler?.popupMenuUserScrolledToEndOfActions()
                }
              }
            })
        }
      }
      .matchedGeometryEffect(id: MenuCustomizationAnimationID.actions, in: namespace)
      .simultaneousGesture(
        DragGesture().onChanged({ _ in
          metricsHandler?.popupMenuScrolledVertically()
        })
      )
      .accessibilityIdentifier(kPopupMenuToolsMenuActionListId)
      .overflowMenuListStyle()
      .onReceive(uiConfiguration.$scrollToAction) { action in
        guard let action = action else {
          return
        }
        withAnimation {
          // Scroll so the item is in the middle of the screen.
          scrollProxy.scrollTo(action.id, anchor: UnitPoint(x: 0.5, y: 0.5))
        }
      }
    }
  }
}

/// Calls `onScroll` when the user performs a drag gesture over the content of the list.
struct ListScrollDetectionModifier: ViewModifier {
  let onScroll: () -> Void
  func body(content: Content) -> some View {
    content
      // For some reason, without this, user interaction is not forwarded to the list.
      .onTapGesture(perform: {})
      // Long press gestures are dismissed and `onPressingChanged` called with
      // `pressing` equal to `false` when the user performs a drag gesture
      // over the content, hence why this works. `DragGesture` cannot be used
      // here, even with a `simultaneousGesture` modifier, because it prevents
      // swipe-to-delete from correctly triggering within the list.
      .onLongPressGesture(
        perform: {},
        onPressingChanged: { pressing in
          if !pressing {
            onScroll()
          }
        })
  }
}
