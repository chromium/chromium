// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI

extension View {
  func crContainerBackground(_ backgroundView: some View) -> some View {
    #if swift(>=5.9)
      if #available(iOS 17.0, *) {
        return containerBackground(for: .widget) {
          backgroundView
        }
      }
    #endif
    return background(backgroundView)
  }

}

extension WidgetConfiguration {
  func crDisfavoredLocations() -> some WidgetConfiguration {
    #if swift(>=5.9)
      if #available(iOS 17.0, *) {
        return disfavoredLocations(
          [.iPhoneWidgetsOnMac], for: [.systemSmall, .systemMedium, .accessoryCircular])
      }
    #endif
    return self
  }

  func crContentMarginsDisabled() -> some WidgetConfiguration {
    #if swift(>=5.9)
      if #available(iOS 17.0, *) {
        return contentMarginsDisabled()
      }
    #endif
    return self
  }

  func crContainerBackgroundRemovable(_ remove: Bool) -> some WidgetConfiguration {
    #if swift(>=5.9)
      if #available(iOS 17.0, *) {
        return containerBackgroundRemovable(remove)
      }
    #endif
    return self
  }
}

#if swift(>=5.9)
  @available(iOS 17.0, *)
  struct CompatBackgroundView<Content: View>: View {
    @Environment(\.showsWidgetContainerBackground) var showsWidgetBackground

    @ViewBuilder let content: (Bool) -> Content

    var body: some View {
      content(showsWidgetBackground)
    }
  }
#endif

extension View {
  // Hides the modified view on iOS17+ if the environment value
  // `\.showsWidgetContainerBackground` is false
  // Note: the property crashes on iOS17 beta 6, so this is not used for now.
  // But keep the wrapper to be used if needed once this is fixed.
  func applyShowWidgetContainerBackground() -> some View {
    #if swift(>=5.9)
      if #available(iOS 17.0, *) {
        return
          CompatBackgroundView { show in
            if show {
              self
            }
          }

      }
    #endif
    return self
  }

}
