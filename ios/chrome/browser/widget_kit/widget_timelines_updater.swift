// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import WidgetKit

/// Helper class to update timelines for WidgetKit widgets.
public final class WidgetTimelinesUpdater: NSObject {
  /// The queue onto which time-consuming work is dispatched.
  private static let queue = DispatchQueue(label: "com.google.chrome.ios.WidgetTimelinesUpdater")

  /// Updates timelines for all shared widgets.
  ///
  /// Whether or not widgets are actually updated depends on many factors
  /// governed by an OS level budgeting system. Widgets are guaranteed
  /// to be updated if the main app is in the foreground.
  ///
  /// This method is safe to call from any thread.
  @objc(reloadAllTimelines)
  public static func reloadAllTimelines() {
    queue.async {
      WidgetCenter.shared.reloadAllTimelines()
    }
  }

  /// Updates timelines of a widget kind.
  @objc(reloadTimelinesOfKind:)
  public static func reloadTimelines(ofKind kind: String) {
    queue.async {
      WidgetCenter.shared.reloadTimelines(ofKind: kind)
    }
  }
}
