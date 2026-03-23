// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import SwiftUI
import UIKit

/// A view that recursively displays a UIMenuElement as a SwiftUI Menu or Button.
@MainActor
struct UIMenuElementView: View {
  let element: UIMenuElement

  /// An optional custom label to use for the menu or button. If nil, a default
  /// label will be generated from the element's title and image.
  var customLabel: AnyView? = nil

  init(element: UIMenuElement) {
    self.element = element
    self.customLabel = nil
  }

  init<L: View>(element: UIMenuElement, @ViewBuilder label: () -> L) {
    self.element = element
    self.customLabel = AnyView(label())
  }

  var body: some View {
    if let menu = element as? UIMenu {
      if menu.options.contains(.displayInline) {
        UIMenuContent(menu: menu)
      } else {
        Menu {
          UIMenuContent(menu: menu)
        } label: {
          if let customLabel = customLabel {
            customLabel
          } else if let image = menu.image {
            Label {
              Text(menu.title)
            } icon: {
              Image(uiImage: image)
            }
          } else {
            Text(menu.title)
          }
        }
      }
    } else if let action = element as? UIAction {
      Button(
        role: action.attributes.contains(.destructive) ? .destructive : nil
      ) {
        UIActionRunner.run(action)
      } label: {
        if let customLabel = customLabel {
          customLabel
        } else if let image = action.image {
          Label {
            Text(action.title)
          } icon: {
            Image(uiImage: image)
          }
        } else {
          Text(action.title)
        }
      }
      .disabled(action.attributes.contains(.disabled))
    }
  }
}

/// A view that displays the content of a UIMenu (its children), optionally
/// wrapped in a section with a header if a title is present.
@MainActor
struct UIMenuContent: View {
  let menu: UIMenu

  var body: some View {
    Section {
      ForEach(menu.children, id: \.self) { child in
        UIMenuElementView(element: child)
      }
    } header: {
      if !menu.title.isEmpty {
        Text(menu.title)
      }
    }
  }
}

/// A helper class to safely run a UIAction without using private APIs.
@MainActor
private class UIActionRunner {
  /// A static button used to trigger the UIAction.
  private static let bridgeButton = UIButton()

  /// Runs the given `action` using a safe UIKit bridge.
  static func run(_ action: UIAction) {
    // Adding the action to a button and then sending the action is a public
    // API way to trigger the action's handler.
    bridgeButton.addAction(action, for: .touchUpInside)
    bridgeButton.sendActions(for: .touchUpInside)
    bridgeButton.removeAction(action, for: .touchUpInside)
  }
}
